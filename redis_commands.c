/* -*- Mode: C; tab-width: 4 -*- */
/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2009 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Original Author: Michael Grunder <michael.grunder@gmail.com          |
  | Maintainer: Nicolas Favre-Felix <n.favre-felix@owlient.eu>           |
  +----------------------------------------------------------------------+
*/

#include "redis_commands.h"
#include <zend_exceptions.h>

/* Generic commands based on method signature and what kind of things we're
 * processing.  Lots of Redis commands take something like key, value, or
 * key, value long.  Each unique signature like this is written only once */

/* A command that takes no arguments */
int redis_empty_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, short *slot,
        void **ctx)
{
    *cmd_len = redis_cmd_format_static(cmd, kw, "");
    return SUCCESS;
}

/* Generic command where we just take a string and do nothing to it*/
int redis_str_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock, char *kw,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    char *arg;
    size_t arg_len;

    // Parse args
    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &arg_len)
            ==FAILURE)
    {
        return FAILURE;
    }

    // Build the command without molesting the string
    *cmd_len = redis_cmd_format_static(cmd, kw, "s", arg, arg_len);

    return SUCCESS;
}

/* Key, long, zval (serialized) */
int redis_key_long_val_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, short *slot,
        void **ctx)
{
    char *key = NULL;
    zend_string *val;
    int val_free, key_free;
    size_t key_len;
    long long expire;
    zval *z_val;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slz", &key, &key_len,
                &expire, &z_val)==FAILURE)
    {
        return FAILURE;
    }

    // Serialize value, prefix key
    val_free = redis_serialize(redis_sock, z_val, &val TSRMLS_CC);
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Construct our command
    *cmd_len = redis_cmd_format_static(cmd, kw, "sls", key, key_len, expire,
            val->val, val->len);

    // Set the slot if directed
    CMD_SET_SLOT(slot,key,key_len);

    if(val_free) zend_string_free(val);
    if(key_free) efree(key);

    return SUCCESS;
}

/* Generic key, long, string (unserialized) */
int redis_key_long_str_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, short *slot,
        void **ctx)
{
    char *key, *val;
    int key_free;
    size_t key_len, val_len;
    long long lval;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sls", &key, &key_len,
                &lval, &val, &val_len)==FAILURE)
    {
        return FAILURE;
    }

    // Prefix our key if requested
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Construct command
    *cmd_len = redis_cmd_format_static(cmd, kw, "sds", key, key_len, (int)lval,
            val, val_len);

    // Set slot
    CMD_SET_SLOT(slot,key,key_len);

    // Free our key if we prefixed
    if(key_free) efree(key);

    return SUCCESS;
}

/* Generic command construction when we just take a key and value */
int redis_kv_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, short *slot,
        void **ctx)
{
    char *key;
    zend_string *val;
    int key_free, val_free;
    size_t key_len;
    zval *z_val;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &key, &key_len,
                &z_val)==FAILURE)
    {
        return FAILURE;
    }

    val_free = redis_serialize(redis_sock, z_val, &val TSRMLS_CC);
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Construct our command
    *cmd_len = redis_cmd_format_static(cmd, kw, "ss", key, key_len, val->val,
            val->len);

    // Set our slot if directed
    CMD_SET_SLOT(slot,key,key_len);

    if(val_free) zend_string_free(val);
    if(key_free) efree(key);

    return SUCCESS;
}

/* Generic command that takes a key and an unserialized value */
int redis_key_str_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, short *slot,
        void **ctx)
{
    char *key, *val;
    int key_free;
    size_t key_len, val_len;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &key, &key_len,
                &val, &val_len)==FAILURE)
    {
        return FAILURE;
    }

    // Prefix key
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Construct command
    *cmd_len = redis_cmd_format_static(cmd, kw, "ss", key, key_len, val,
            val_len);

    // Set slot if directed
    CMD_SET_SLOT(slot,key,key_len);

    return SUCCESS;
}

/* Key, string, string without serialization (ZCOUNT, ZREMRANGEBYSCORE) */
int redis_key_str_str_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, short *slot,
        void **ctx)
{
    char *key, *val1, *val2;
    int key_free;
    size_t key_len, val1_len, val2_len;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss", &key, &key_len,
                &val1, &val1_len, &val2, &val2_len)==FAILURE)
    {
        return FAILURE;
    }

    // Prefix key
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Construct command
    *cmd_len = redis_cmd_format_static(cmd, kw, "sss", key, key_len, val1,
            val1_len, val2, val2_len);

    // Set slot
    CMD_SET_SLOT(slot,key,key_len);

    // Free key if prefixed
    if(key_free) efree(key);

    // Success!
    return SUCCESS;
}

/* Generic command that takes two keys */
int redis_key_key_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, short *slot,
        void **ctx)
{
    char *key1, *key2;
    size_t key1_len, key2_len;
    int key1_free, key2_free;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &key1, &key1_len,
                &key2, &key2_len)==FAILURE)
    {
        return FAILURE;
    }

    // Prefix both keys
    key1_free = redis_key_prefix(redis_sock, &key1, &key1_len);
    key2_free = redis_key_prefix(redis_sock, &key2, &key2_len);

    // If a slot is requested, we can test that they hash the same
    if(slot) {
        // Slots where these keys resolve
        short slot1 = cluster_hash_key(key1, key1_len);
        short slot2 = cluster_hash_key(key2, key2_len);

        // Check if Redis would give us a CROSSLOT error
        if(slot1 != slot2) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING,
                    "Keys don't hash to the same slot");
            if(key1_free) efree(key1);
            if(key2_free) efree(key2);
            return FAILURE;
        }

        // They're both the same
        *slot = slot1;
    }

    // Construct our command
    *cmd_len = redis_cmd_format_static(cmd, kw, "ss", key1, key1_len, key2,
            key2_len);

    return SUCCESS;
}

/* Generic command construction where we take a key and a long */
int redis_key_long_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, short *slot,
        void **ctx)
{
    char *key;
    int key_free;
    long long lval;
    size_t key_len;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &key, &key_len,
                &lval)==FAILURE)
    {
        return FAILURE;
    }

    // Prefix key
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Disallow zero length keys (for now)
    if(key_len == 0) {
        if(key_free) efree(key);
        return FAILURE;
    }

    // Construct our command
    *cmd_len = redis_cmd_format_static(cmd, kw, "sl", key, key_len, lval);

    // Set slot if directed
    CMD_SET_SLOT(slot, key, key_len);

    // Success!
    return SUCCESS;
}

/* key, long, long */
int redis_key_long_long_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, short *slot,
        void **ctx)
{
    char *key;
    int key_free;
    size_t key_len;
    long long val1, val2;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sll", &key, &key_len,
                &val1, &val2)==FAILURE)
    {
        return FAILURE;
    }

    // Prefix our key
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Construct command
    *cmd_len = redis_cmd_format_static(cmd, kw, "sll", key, key_len, val1,
            val2);

    // Set slot
    CMD_SET_SLOT(slot,key,key_len);

    if(key_free) efree(key);

    return SUCCESS;
}

/* Generic command where we take a single key */
int redis_key_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, short *slot,
        void **ctx)
{
    char *key;
    size_t key_len;
    int key_free;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len)
            ==FAILURE)
    {
        return FAILURE;
    }

    // Prefix our key
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Construct our command
    *cmd_len = redis_cmd_format_static(cmd, kw, "s", key, key_len);

    // Set slot if directed
    CMD_SET_SLOT(slot,key,key_len);

    if(key_free) efree(key);

    return SUCCESS;
}

/* Generic command where we take a key and a double */
int redis_key_dbl_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, short *slot,
        void **ctx)
{
    char *key;
    size_t key_len;
    int key_free;
    double val;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sd", &key, &key_len,
                &val)==FAILURE)
    {
        return FAILURE;
    }

    // Prefix our key
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Construct our command
    *cmd_len = redis_cmd_format_static(cmd, kw, "sf", key, key_len, val);

    // Set slot if directed
    CMD_SET_SLOT(slot,key,key_len);

    if(key_free) efree(key);

    return SUCCESS;
}

/* Generic to construct SCAN and variant commands */
int redis_fmt_scan_cmd(char **cmd, REDIS_SCAN_TYPE type, char *key, int key_len,
        long it, char *pat, int pat_len, long count)
{
    static char *kw[] = {"SCAN","SSCAN","HSCAN","ZSCAN"};
    int argc;
    smart_str cmdstr = {0};

    // Figure out our argument count
    argc = 1 + (type!=TYPE_SCAN) + (pat_len>0?2:0) + (count>0?2:0);

    redis_cmd_init_sstr(&cmdstr, argc, kw[type], strlen(kw[type]));

    // Append our key if it's not a regular SCAN command
    if(type != TYPE_SCAN) {
        redis_cmd_append_sstr(&cmdstr, key, key_len);
    }

    // Append cursor
    redis_cmd_append_sstr_long(&cmdstr, it);

    // Append count if we've got one
    if(count) {
        redis_cmd_append_sstr(&cmdstr,"COUNT",sizeof("COUNT")-1);
        redis_cmd_append_sstr_long(&cmdstr, count);
    }

    // Append pattern if we've got one
    if(pat_len) {
        redis_cmd_append_sstr(&cmdstr,"MATCH",sizeof("MATCH")-1);
        redis_cmd_append_sstr(&cmdstr,pat,pat_len);
    }

    // Push command to the caller, return length
    *cmd = cmdstr.s->val;
    return cmdstr.s->len;
}

/* ZRANGE/ZREVRANGE */
int redis_zrange_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, int *withscores,
        short *slot, void **ctx)
{
    char *key;
    int key_len, key_free;
    long start, end;
    zend_bool ws=0;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sll|b", &key, &key_len,
                &start, &end, &ws)==FAILURE)
    {
        return FAILURE;
    }

    key_free = redis_key_prefix(redis_sock, &key, &key_len);
    if(ws) {
        *cmd_len = redis_cmd_format_static(cmd, kw, "sdds", key, key_len, start,
                end, "WITHSCORES", sizeof("WITHSCORES")-1);
    } else {
        *cmd_len = redis_cmd_format_static(cmd, kw, "sdd", key, key_len, start,
                end);
    }

    CMD_SET_SLOT(slot, key, key_len);

    // Free key, push out WITHSCORES option
    if(key_free) efree(key);
    *withscores = ws;

    return SUCCESS;
}

/* ZRANGEBYSCORE/ZREVRANGEBYSCORE */
int redis_zrangebyscore_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, int *withscores,
        short *slot, void **ctx)
{
    char *key;
    int key_len, key_free;
    char *start, *end;
    int start_len, end_len;
    int has_limit=0;
    long limit_low, limit_high;
    zval *z_opt=NULL, *z_ele;
    HashTable *ht_opt;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss|a", &key, &key_len,
                &start, &start_len, &end, &end_len, &z_opt)
            ==FAILURE)
    {
        return FAILURE;
    }

    // Check for an options array
    if(z_opt && Z_TYPE_P(z_opt)==IS_ARRAY) {
        ht_opt = Z_ARRVAL_P(z_opt);

        // Check for WITHSCORES
        *withscores = ((z_ele = zend_hash_str_find(ht_opt,"withscores",sizeof("withscores") - 1)) != NULL
                && Z_TYPE_P(z_ele)==IS_TRUE);

        // LIMIT
        if ((z_ele = zend_hash_str_find(ht_opt,"limit",sizeof("limit") - 1)) != NULL) {
            HashTable *ht_limit = Z_ARRVAL_P(z_ele);
            zval *z_off, *z_cnt;
            if((z_cnt = zend_hash_index_find(ht_limit,0))!=NULL &&
                    (z_off = zend_hash_index_find(ht_limit,1))!=NULL &&
                    Z_TYPE_P(z_off)==IS_LONG && Z_TYPE_P(z_cnt)==IS_LONG)
            {
                has_limit  = 1;
                limit_low  = Z_LVAL_P(z_off);
                limit_high = Z_LVAL_P(z_cnt);
            }
        }
    }

    // Prefix our key, set slot
    key_free = redis_key_prefix(redis_sock, &key, &key_len);
    CMD_SET_SLOT(slot,key,key_len);

    // Construct our command
    if(*withscores) {
        if(has_limit) {
            *cmd_len = redis_cmd_format_static(cmd, kw, "ssssdds", key, key_len,
                    start, start_len, end, end_len, "LIMIT", 5, limit_low,
                    limit_high, "WITHSCORES", 10);
        } else {
            *cmd_len = redis_cmd_format_static(cmd, kw, "ssss", key, key_len,
                    start, start_len, end, end_len, "WITHSCORES", 10);
        }
    } else {
        if(has_limit) {
            *cmd_len = redis_cmd_format_static(cmd, kw, "ssssdd", key, key_len,
                    start, start_len, end, end_len, "LIMIT", 5, limit_low,
                    limit_high);
        } else {
            *cmd_len = redis_cmd_format_static(cmd, kw, "sss", key, key_len,
                    start, start_len, end, end_len);
        }
    }

    // Free our key if we prefixed
    if(key_free) efree(key);

    return SUCCESS;
}

/* ZUNIONSTORE, ZINTERSTORE */
int redis_zinter_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, short *slot,
        void **ctx)
{
    char *key, *agg_op=NULL;
    int key_free, key_len;
    zval *z_keys, *z_weights=NULL, *z_ele;
    HashTable *ht_keys, *ht_weights=NULL;
    smart_str cmdstr = {0};
    int argc = 2, agg_op_len=0, keys_count;

    // Parse args
    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa|a!s", &key,
                &key_len, &z_keys, &z_weights, &agg_op,
                &agg_op_len)==FAILURE)
    {
        return FAILURE;
    }

    // Grab our keys
    ht_keys = Z_ARRVAL_P(z_keys);

    // Nothing to do if there aren't any
    if((keys_count = zend_hash_num_elements(ht_keys))==0) {
        return FAILURE;
    } else {
        argc += keys_count;
    }

    // Handle WEIGHTS
    if(z_weights != NULL) {
        ht_weights = Z_ARRVAL_P(z_weights);
        if(zend_hash_num_elements(ht_weights) != keys_count) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING,
                    "WEIGHTS and keys array should be the same size!");
            return FAILURE;
        }

        // "WEIGHTS" + key count
        argc += keys_count + 1;
    }

    // AGGREGATE option
    if(agg_op_len != 0) {
        if(strncasecmp(agg_op, "SUM", sizeof("SUM")) &&
                strncasecmp(agg_op, "MIN", sizeof("MIN")) &&
                strncasecmp(agg_op, "MAX", sizeof("MAX")))
        {
            php_error_docref(NULL TSRMLS_CC, E_WARNING,
                    "Invalid AGGREGATE option provided!");
            return FAILURE;
        }

        // "AGGREGATE" + type
        argc += 2;
    }

    // Prefix key
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Start building our command
    redis_cmd_init_sstr(&cmdstr, argc, kw, strlen(kw));
    redis_cmd_append_sstr(&cmdstr, key, key_len);
    redis_cmd_append_sstr_int(&cmdstr, keys_count);

    // Set our slot, free the key if we prefixed it
    CMD_SET_SLOT(slot,key,key_len);
    if(key_free) efree(key);

    // Process input keys
    ZEND_HASH_FOREACH_VAL(ht_keys, z_ele) {
        char *key;
        int key_free, key_len;
        zval z_tmp;

        if(Z_TYPE_P(z_ele) == IS_STRING) {
            key = Z_STRVAL_P(z_ele);
            key_len = Z_STRLEN_P(z_ele);
        } else {
            ZVAL_COPY(&z_tmp, z_ele);
            convert_to_string(&z_tmp);

            key = Z_STRVAL(z_tmp);
            key_len = Z_STRLEN(z_tmp);
        }

        // Prefix key if necissary
        key_free = redis_key_prefix(redis_sock, &key, &key_len);

        // If we're in Cluster mode, verify the slot is the same
        if(slot && *slot != cluster_hash_key(key,key_len)) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING,
                    "All keys don't hash to the same slot!");
            zend_string_release(cmdstr.s);
            if(key_free) efree(key);
            if(&z_tmp) {
                zval_dtor(&z_tmp);
                efree(&z_tmp);
            }
            return FAILURE;
        }

        // Append this input set
        redis_cmd_append_sstr(&cmdstr, key, key_len);

        // Cleanup
        if(key_free) efree(key);
        if(&z_tmp) {
            zval_dtor(&z_tmp);
        }
    } ZEND_HASH_FOREACH_END();

    // Weights
    if(ht_weights != NULL) {
        redis_cmd_append_sstr(&cmdstr, "WEIGHTS", sizeof("WEIGHTS")-1);

        // Process our weights
        ZEND_HASH_FOREACH_VAL(ht_weights, z_ele) {
            // Ignore non numeric args unless they're inf/-inf
            if(Z_TYPE_P(z_ele)!=IS_LONG && Z_TYPE_P(z_ele)!=IS_DOUBLE &&
                    strncasecmp(Z_STRVAL_P(z_ele),"inf",sizeof("inf"))!=0 &&
                    strncasecmp(Z_STRVAL_P(z_ele),"-inf",sizeof("-inf"))!=0 &&
                    strncasecmp(Z_STRVAL_P(z_ele),"+inf",sizeof("+inf"))!=0)
            {
                php_error_docref(NULL TSRMLS_CC, E_WARNING,
                        "Weights must be numeric or '-inf','inf','+inf'");
                zend_string_release(cmdstr.s);
                return FAILURE;
            }

            switch(Z_TYPE_P(z_ele)) {
                case IS_LONG:
                    redis_cmd_append_sstr_long(&cmdstr, Z_LVAL_P(z_ele));
                    break;
                case IS_DOUBLE:
                    redis_cmd_append_sstr_dbl(&cmdstr, Z_DVAL_P(z_ele));
                    break;
                case IS_STRING:
                    redis_cmd_append_sstr(&cmdstr, Z_STRVAL_P(z_ele),
                            Z_STRLEN_P(z_ele));
                    break;
            }
        } ZEND_HASH_FOREACH_END();
    }

    // AGGREGATE
    if(agg_op_len != 0) {
        redis_cmd_append_sstr(&cmdstr, "AGGREGATE", sizeof("AGGREGATE")-1);
        redis_cmd_append_sstr(&cmdstr, agg_op, agg_op_len);
    }

    // Push out values
    *cmd     = cmdstr.s->val;
    *cmd_len = cmdstr.s->len;

    return SUCCESS;
}

/* SUBSCRIBE/PSUBSCRIBE */
int redis_subscribe_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, short *slot,
        void **ctx)
{
    zval *z_arr, *z_chan;
    HashTable *ht_chan;
    smart_str cmdstr = {0};
    subscribeContext *sctx = emalloc(sizeof(subscribeContext));
    int key_len, key_free;
    char *key;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "af", &z_arr,
                &(sctx->cb), &(sctx->cb_cache))==FAILURE)
    {
        efree(sctx);
        return FAILURE;
    }

    ht_chan    = Z_ARRVAL_P(z_arr);
    sctx->kw   = kw;
    sctx->argc = zend_hash_num_elements(ht_chan);

    if(sctx->argc==0) {
        efree(sctx);
        return FAILURE;
    }

    // Start command construction
    redis_cmd_init_sstr(&cmdstr, sctx->argc, kw, strlen(kw));

    // Iterate over channels
    ZEND_HASH_FOREACH_VAL(ht_chan, z_chan) {
        // We want to deal with strings here
        convert_to_string(z_chan);

        // Grab channel name, prefix if required
        key      = Z_STRVAL_P(z_chan);
        key_len  = Z_STRLEN_P(z_chan);
        key_free = redis_key_prefix(redis_sock, &key, &key_len);

        // Add this channel
        redis_cmd_append_sstr(&cmdstr, key, key_len);

        // Free our key if it was prefixed
        if(key_free) efree(key);

    } ZEND_HASH_FOREACH_END();

    // Push values out
    *cmd_len = cmdstr.s->len;
    *cmd     = cmdstr.s->val;
    *ctx     = (void*)sctx;

    // Pick a slot at random
    CMD_RAND_SLOT(slot);

    return SUCCESS;
}

/* UNSUBSCRIBE/PUNSUBSCRIBE */
int redis_unsubscribe_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, short *slot,
        void **ctx)
{
    zval *z_arr, *z_chan;
    HashTable *ht_arr;
    smart_str cmdstr = {0};
    subscribeContext *sctx = emalloc(sizeof(subscribeContext));

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &z_arr)==FAILURE) {
        efree(sctx);
        return FAILURE;
    }

    ht_arr = Z_ARRVAL_P(z_arr);

    sctx->argc = zend_hash_num_elements(ht_arr);
    if(sctx->argc == 0) {
        efree(sctx);
        return FAILURE;
    }

    redis_cmd_init_sstr(&cmdstr, sctx->argc, kw, strlen(kw));

    ZEND_HASH_FOREACH_VAL(ht_arr, z_chan) {
        char *key = Z_STRVAL_P(z_chan);
        int key_len = Z_STRLEN_P(z_chan), key_free;

        key_free = redis_key_prefix(redis_sock, &key, &key_len);
        redis_cmd_append_sstr(&cmdstr, key, key_len);
        if(key_free) efree(key);
    } ZEND_HASH_FOREACH_END();

    // Push out vals
    *cmd_len = cmdstr.s->len;
    *cmd     = cmdstr.s->val;
    *ctx     = (void*)sctx;

    return SUCCESS;
}

/* ZRANGEBYLEX/ZREVRANGEBYLEX */
int redis_zrangebylex_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, short *slot,
        void **ctx)
{
    char *key, *min, *max;
    int key_len, min_len, max_len, key_free;
    long offset, count;
    int argc = ZEND_NUM_ARGS();

    /* We need either 3 or 5 arguments for this to be valid */
    if(argc != 3 && argc != 5) {
        php_error_docref(0 TSRMLS_CC, E_WARNING,
                "Must pass either 3 or 5 arguments");
        return FAILURE;
    }

    if(zend_parse_parameters(argc TSRMLS_CC, "sss|ll", &key,
                &key_len, &min, &min_len, &max, &max_len,
                &offset, &count)==FAILURE)
    {
        return FAILURE;
    }

    /* min and max must start with '(' or '[' */
    if(min_len < 1 || max_len < 1 || (min[0] != '(' && min[0] != '[') ||
            (max[0] != '(' && max[0] != '['))
    {
        php_error_docref(0 TSRMLS_CC, E_WARNING,
                "min and max arguments must start with '[' or '('");
        return FAILURE;
    }

    /* Prefix key */
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    /* Construct command */
    if(argc == 3) {
        *cmd_len = redis_cmd_format_static(cmd, kw, "sss", key, key_len, min, 
                min_len, max, max_len);
    } else {
        *cmd_len = redis_cmd_format_static(cmd, kw, "ssssll", key, key_len, min, 
                min_len, max, max_len, "LIMIT", sizeof("LIMIT")-1, offset, count);
    }

    /* Pick our slot */
    CMD_SET_SLOT(slot,key,key_len);

    /* Free key if we prefixed */
    if(key_free) efree(key);

    return SUCCESS;
}

/* ZLEXCOUNT/ZREMRANGEBYLEX */
int redis_gen_zlex_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, short *slot, 
        void **ctx)
{
    char *key, *min, *max;
    int key_len, min_len, max_len, key_free;

    /* Parse args */
    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss", &key, &key_len,
                &min, &min_len, &max, &max_len)==FAILURE)
    {
        return FAILURE;
    }

    /* Quick sanity check on min/max */
    if(min_len<1 || max_len<1 || (min[0]!='(' && min[0]!='[') ||
            (max[0]!='(' && max[0]!='['))
    {
        php_error_docref(NULL TSRMLS_CC, E_WARNING,
                "Min and Max arguments must begin with '(' or '['");
        return FAILURE;
    }

    /* Prefix key if we need to */
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    /* Construct command */
    *cmd_len = redis_cmd_format_static(cmd, kw, "sss", key, key_len, min, 
            min_len, max, max_len);

    /* set slot */
    CMD_SET_SLOT(slot,key,key_len);

    /* Free key if prefixed */
    if(key_free) efree(key);

    return SUCCESS;
}

/* Commands that take a key followed by a variable list of serializable
 * values (RPUSH, LPUSH, SADD, SREM, etc...) */
int redis_key_varval_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, short *slot,
        void **ctx)
{
    zval *z_args;
    smart_str cmdstr = {0};
    zend_string *arg;
    char *tmp_arg;
    int arg_free, tmp_arg_len, i;
    int argc = ZEND_NUM_ARGS();

    // We at least need a key and one value
    if(argc < 2) {
        return FAILURE;
    }

    // Make sure we at least have a key, and we can get other args
    z_args = safe_emalloc(sizeof(zval), argc, 0);
    if(zend_get_parameters_array(ht, argc, z_args)==FAILURE) {
        efree(z_args);
        return FAILURE;
    }

    // Grab the first argument (our key) as a string
    convert_to_string(&z_args[0]);
    tmp_arg = Z_STRVAL(z_args[0]);
    tmp_arg_len = Z_STRLEN(z_args[0]);

    // Prefix if required
    arg_free = redis_key_prefix(redis_sock, &tmp_arg, &tmp_arg_len);

    // Start command construction
    redis_cmd_init_sstr(&cmdstr, argc, kw, strlen(kw));
    redis_cmd_append_sstr(&cmdstr, tmp_arg, tmp_arg_len);

    // Set our slot, free key prefix if we prefixed it
    CMD_SET_SLOT(slot,tmp_arg,tmp_arg_len);
    if(arg_free) efree(tmp_arg);

    // Add our members
    for(i=1;i<argc;i++) {
        arg_free = redis_serialize(redis_sock, &z_args[i], &arg TSRMLS_CC);
        redis_cmd_append_sstr(&cmdstr, arg->val, arg->len);
        if(arg_free) zend_string_free(arg);
    }

    // Push out values
    *cmd     = cmdstr.s->val;
    *cmd_len = cmdstr.s->len;

    // Cleanup arg array
    efree(z_args);

    // Success!
    return SUCCESS;
}

/* Generic function that takes a variable number of keys, with an optional
 * timeout value.  This can handle various SUNION/SUNIONSTORE/BRPOP type
 * commands. */
static int gen_varkey_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, int kw_len, int min_argc, int has_timeout,
        char **cmd, int *cmd_len, short *slot)
{
    zval *z_args, *z_ele;
    HashTable *ht_arr;
    char *key;
    int key_free, key_len, i, tail;
    int single_array = 0, argc = ZEND_NUM_ARGS();
    smart_str cmdstr = {0};
    long timeout;
    short kslot = -1;

    if(argc < min_argc) {
        zend_wrong_param_count(TSRMLS_C);
        return FAILURE;
    }

    // Allocate args
    z_args = safe_emalloc(sizeof(zval), argc, 0);
    if(zend_get_parameters_array(ht, argc, z_args)==FAILURE) {
        efree(z_args);
        return FAILURE;
    }

    // Handle our "single array" case
    if(has_timeout == 0) {
        single_array = argc==1 && Z_TYPE(z_args[0])==IS_ARRAY;
    } else {
        single_array = argc==2 && Z_TYPE(z_args[0])==IS_ARRAY &&
            Z_TYPE(z_args[1])==IS_LONG;
        timeout = Z_LVAL(z_args[1]);
    }

    // If we're running a single array, rework args
    if(single_array) {
        ht_arr = Z_ARRVAL(z_args[0]);
        argc = zend_hash_num_elements(ht_arr);
        if(has_timeout) argc++;
        efree(z_args);
        z_args = NULL;

        /* If the array is empty, we can simply abort */
        if (argc == 0) return FAILURE;
    }

    // Begin construction of our command
    redis_cmd_init_sstr(&cmdstr, argc, kw, kw_len);

    if(single_array) {
        ZEND_HASH_FOREACH_VAL(ht_arr, z_ele) {
            convert_to_string(z_ele);
            key = Z_STRVAL_P(z_ele);
            key_len = Z_STRLEN_P(z_ele);
            key_free = redis_key_prefix(redis_sock, &key, &key_len);

            // Protect against CROSSLOT errors
            if(slot) {
                if(kslot == -1) {
                    kslot = cluster_hash_key(key, key_len);
                } else if(cluster_hash_key(key,key_len)!=kslot) {
                    php_error_docref(NULL TSRMLS_CC, E_WARNING,
                            "Not all keys hash to the same slot!");
                    return FAILURE;
                }
            }

            // Append this key, free it if we prefixed
            redis_cmd_append_sstr(&cmdstr, key, key_len);
            if(key_free) efree(key);
        } ZEND_HASH_FOREACH_END();

        if(has_timeout) {
            redis_cmd_append_sstr_long(&cmdstr, timeout);
        }
    } else {
        if(has_timeout && Z_TYPE(z_args[argc-1])!=IS_LONG) {
            php_error_docref(NULL TSRMLS_CC, E_ERROR,
                    "Timeout value must be a LONG");
            efree(z_args);
            return FAILURE;
        }

        tail = has_timeout ? argc-1 : argc;
        for(i=0;i<tail;i++) {
            convert_to_string(&z_args[i]);
            key = Z_STRVAL(z_args[i]);
            key_len = Z_STRLEN(z_args[i]);

            key_free = redis_key_prefix(redis_sock, &key, &key_len);

            /* Protect against CROSSSLOT errors if we've got a slot */
            if (slot) {
                if( kslot == -1) {
                    kslot = cluster_hash_key(key, key_len);
                } else if(cluster_hash_key(key,key_len)!=kslot) {
                    php_error_docref(NULL TSRMLS_CC, E_WARNING,
                        "Not all keys hash to the same slot");
                    efree(z_args);
                    return FAILURE;
                }
            }

            // Append this key
            redis_cmd_append_sstr(&cmdstr, key, key_len);
            if(key_free) efree(key);
        }
        if(has_timeout) {
            redis_cmd_append_sstr_long(&cmdstr, Z_LVAL(z_args[tail]));
        }

        // Cleanup args
        efree(z_args);
    }

    // Push out parameters
    if(slot) *slot = kslot;
    *cmd = cmdstr.s->val;
    *cmd_len = cmdstr.s->len;

    return SUCCESS;
}

/*
 * Commands with specific signatures or that need unique functions because they
 * have specific processing (argument validation, etc) that make them unique
 */

/* SET */
int redis_set_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    zval *z_value, *z_opts=NULL;
    char *key = NULL, *exp_type = NULL, *set_type = NULL;
    zend_string *val;
    size_t key_len;
    int key_free, val_free;
    long expire = -1;

    // Make sure the function is being called correctly
    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz|z", &key, &key_len,
                &z_value, &z_opts)==FAILURE)
    {
        return FAILURE;
    }

    /* Our optional argument can either be a long (to support legacy SETEX */
    /* redirection), or an array with Redis >= 2.6.12 set options */
    if(z_opts && Z_TYPE_P(z_opts) != IS_LONG && Z_TYPE_P(z_opts) != IS_ARRAY
       && Z_TYPE_P(z_opts) != IS_NULL)
    {
        return FAILURE;
    }

    // Serialize and key prefix if required
    val_free = redis_serialize(redis_sock, z_value, &val TSRMLS_CC);
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Check for an options array
    if(z_opts && Z_TYPE_P(z_opts) == IS_ARRAY) {
        HashTable *kt = Z_ARRVAL_P(z_opts);
        int type;
        unsigned long idx;
        zend_string *k;
        zval *v;

        /* Iterate our option array */
        ZEND_HASH_FOREACH_KEY_VAL(kt, idx, k, v) {
            if (k && (Z_TYPE_P(v) == IS_LONG) &&
                    (Z_LVAL_P(v) > 0) && IS_EX_PX_ARG(k->val)) {
                exp_type = k->val;
                expire = Z_LVAL_P(v);
            } else if (Z_TYPE_P(v) == IS_STRING &&
                    IS_NX_XX_ARG(Z_STRVAL_P(v))) {
                set_type = Z_STRVAL_P(v);
            }
        } ZEND_HASH_FOREACH_END();
    } else if(z_opts && Z_TYPE_P(z_opts) == IS_LONG) {
        expire = Z_LVAL_P(z_opts);
    }

    /* Now let's construct the command we want */
    if(exp_type && set_type) {
        /* SET <key> <value> NX|XX PX|EX <timeout> */
        *cmd_len = redis_cmd_format_static(cmd, "SET", "ssssl", key, key_len,
                val->val, val->len, set_type, 2, exp_type,
                2, expire);
    } else if(exp_type) {
        /* SET <key> <value> PX|EX <timeout> */
        *cmd_len = redis_cmd_format_static(cmd, "SET", "sssl", key, key_len,
                val->val, val->len, exp_type, 2, expire);
    } else if(set_type) {
        /* SET <key> <value> NX|XX */
        *cmd_len = redis_cmd_format_static(cmd, "SET", "sss", key, key_len, val->val,
                val->len, set_type, 2);
    } else if(expire > 0) {
        /* Backward compatible SETEX redirection */
        *cmd_len = redis_cmd_format_static(cmd, "SETEX", "sls", key, key_len,
                expire, val->val, val->len);
    } else {
        /* SET <key> <value> */
        *cmd_len = redis_cmd_format_static(cmd, "SET", "ss", key, key_len, val->val,
                val->len);
    }

    // If we've been passed a slot pointer, return the key's slot
    CMD_SET_SLOT(slot,key,key_len);

    if(key_free) efree(key);
    if(val_free) zend_string_release(val);

    return SUCCESS;
}

/* BRPOPLPUSH */
int redis_brpoplpush_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    char *key1, *key2;
    int key1_len, key2_len;
    int key1_free, key2_free;
    short slot1, slot2;
    long timeout;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssl", &key1, &key1_len,
                &key2, &key2_len, &timeout)==FAILURE)
    {
        return FAILURE;
    }

    // Key prefixing
    key1_free = redis_key_prefix(redis_sock, &key1, &key1_len);
    key2_free = redis_key_prefix(redis_sock, &key2, &key2_len);

    // In cluster mode, verify the slots match
    if(slot) {
        slot1 = cluster_hash_key(key1, key1_len);
        slot2 = cluster_hash_key(key2, key2_len);
        if(slot1 != slot2) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING,
                    "Keys hash to different slots!");
            if(key1_free) efree(key1);
            if(key2_free) efree(key2);
            return FAILURE;
        }

        // Both slots are the same
        *slot = slot1;
    }

    // Consistency with Redis, if timeout < 0 use RPOPLPUSH
    if(timeout < 0) {
        *cmd_len = redis_cmd_format_static(cmd, "RPOPLPUSH", "ss", key1,
                key1_len, key2, key2_len);
    } else {
        *cmd_len = redis_cmd_format_static(cmd, "BRPOPLPUSH", "ssd", key1,
                key1_len, key2, key2_len, timeout);
    }

    return SUCCESS;
}

/* To maintain backward compatibility with earlier versions of phpredis, we 
 * allow for an optional "increment by" argument for INCR and DECR even though
 * that's not how Redis proper works */
#define TYPE_INCR 0
#define TYPE_DECR 1

/* Handle INCR(BY) and DECR(BY) depending on optional increment value */
static int 
redis_atomic_increment(INTERNAL_FUNCTION_PARAMETERS, int type, 
                       RedisSock *redis_sock, char **cmd, int *cmd_len, 
                       short *slot, void **ctx)
{
    char *key;
    int key_free;
    size_t key_len;
    long val = 1;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &key, &key_len,
                              &val)==FAILURE)
    {
        return FAILURE;
    }

    /* Prefix the key if required */
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    /* If our value is 1 we use INCR/DECR.  For other values, treat the call as
     * an INCRBY or DECRBY call */
    if (type == TYPE_INCR) {
        if (val == 1) {
           *cmd_len = redis_cmd_format_static(cmd,"INCR","s",key,key_len);
        } else {
           *cmd_len = redis_cmd_format_static(cmd,"INCRBY","sd",key,key_len,val);
        }
    } else {
        if (val == 1) {
            *cmd_len = redis_cmd_format_static(cmd,"DECR","s",key,key_len);
        } else {
            *cmd_len = redis_cmd_format_static(cmd,"DECRBY","sd",key,key_len,val);
        }
    }

    /* Set our slot */
    CMD_SET_SLOT(slot,key,key_len);

    /* Free our key if we prefixed */
    if (key_free) efree(key);

    /* Success */
    return SUCCESS;
}
 
/* INCR */
int redis_incr_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
                   char **cmd, int *cmd_len, short *slot, void **ctx)
{
    return redis_atomic_increment(INTERNAL_FUNCTION_PARAM_PASSTHRU,
        TYPE_INCR, redis_sock, cmd, cmd_len, slot, ctx);
}

/* DECR */
int redis_decr_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
                   char **cmd, int *cmd_len, short *slot, void **ctx)
{
    return redis_atomic_increment(INTERNAL_FUNCTION_PARAM_PASSTHRU,
        TYPE_DECR, redis_sock, cmd, cmd_len, slot, ctx);
}

/* HINCRBY */
int redis_hincrby_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    char *key, *mem;
    int key_len, mem_len, key_free;
    long byval;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssl", &key, &key_len,
                &mem, &mem_len, &byval)==FAILURE)
    {
        return FAILURE;
    }

    // Prefix our key if necissary
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Construct command
    *cmd_len = redis_cmd_format_static(cmd, "HINCRBY", "ssd", key, key_len, mem,
            mem_len, byval);
    // Set slot
    CMD_SET_SLOT(slot,key,key_len);

    /* Free the key if we prefixed */
    if (key_free) efree(key);

    // Success
    return SUCCESS;
}

/* HINCRBYFLOAT */
int redis_hincrbyfloat_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    char *key, *mem;
    int key_len, mem_len, key_free;
    double byval;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssd", &key, &key_len,
                &mem, &mem_len, &byval)==FAILURE)
    {
        return FAILURE;
    }

    // Prefix key
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Construct command
    *cmd_len = redis_cmd_format_static(cmd, "HINCRBYFLOAT", "ssf", key, key_len,
            mem, mem_len, byval);

    // Set slot
    CMD_SET_SLOT(slot,key,key_len);

    /* Free the key if we prefixed */
    if (key_free) efree(key);

    // Success
    return SUCCESS;
}

/* HMGET */
int redis_hmget_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    char *key;
    zval *z_arr, **z_mems, *z_mem;
    int i, count, valid=0, key_len, key_free;
    HashTable *ht_arr;
    smart_str cmdstr = {0};

    // Parse arguments
    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa", &key, &key_len,
                &z_arr)==FAILURE)
    {
        return FAILURE;
    }

    // Our HashTable
    ht_arr = Z_ARRVAL_P(z_arr);

    // We can abort if we have no elements
    if((count = zend_hash_num_elements(ht_arr))==0) {
        return FAILURE;
    }

    // Prefix our key
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Allocate memory for mems+1 so we can have a sentinel
    z_mems = ecalloc(count+1, sizeof(zval *));

    // Iterate over our member array
    ZEND_HASH_FOREACH_VAL(ht_arr, z_mem) {
        // We can only handle string or long values here
        if ((Z_TYPE_P(z_mem)==IS_STRING && Z_STRLEN_P(z_mem)>0) 
            || Z_TYPE_P(z_mem)==IS_LONG) 
        {
            // Copy into our member array
            z_mems[valid] = (zval *) emalloc(sizeof(zval));
            //Z_REFCOUNT_P(z_mems[valid]) = 1;
            Z_REF_P(z_mems[valid]) = 0;
            ZVAL_DUP(z_mems[valid], z_mem);
            convert_to_string(z_mems[valid]);

            // Increment the member count to actually send
            valid++;
        }
    } ZEND_HASH_FOREACH_END();

    // If nothing was valid, fail
    if(valid == 0) {
        if(key_free) efree(key);
        efree(z_mems);
        return FAILURE;
    }

    // Sentinel so we can free this even if it's used and then we discard
    // the transaction manually or there is a transaction failure
    z_mems[valid]=NULL;

    // Start command construction
    redis_cmd_init_sstr(&cmdstr, valid+1, "HMGET", sizeof("HMGET")-1);
    redis_cmd_append_sstr(&cmdstr, key, key_len);

    // Iterate over members, appending as arguments
    for(i=0;i<valid;i++) {
        redis_cmd_append_sstr(&cmdstr, Z_STRVAL_P(z_mems[i]),
                Z_STRLEN_P(z_mems[i]));
    }

    // Set our slot
    CMD_SET_SLOT(slot,key,key_len);

    // Free our key if we prefixed it
    if(key_free) efree(key);

    // Push out command, length, and key context
    *cmd     = cmdstr.s->val;
    *cmd_len = cmdstr.s->len;
    *ctx     = (void*)z_mems;

    // Success!
    return SUCCESS;
}

/* HMSET */
int redis_hmset_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    char *key;
    int key_len, key_free, count;
    unsigned long idx;
    zval *z_arr;
    HashTable *ht_vals;
    smart_str cmdstr = {0};
    zend_string *mem;
    zval *z_val;

    // Parse args
    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa", &key, &key_len,
                &z_arr)==FAILURE)
    {
        return FAILURE;
    }

    // We can abort if we have no fields
    if((count = zend_hash_num_elements(Z_ARRVAL_P(z_arr)))==0) {
        return FAILURE;
    }

    // Prefix our key
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Grab our array as a HashTable
    ht_vals = Z_ARRVAL_P(z_arr);

    // Initialize our HMSET command (key + 2x each array entry), add key
    redis_cmd_init_sstr(&cmdstr, 1+(count*2), "HMSET", sizeof("HMSET")-1);
    redis_cmd_append_sstr(&cmdstr, key, key_len);

    // Start traversing our key => value array
    ZEND_HASH_FOREACH_KEY_VAL(ht_vals, idx, mem, z_val) {
        zend_string *val;
        char kbuf[40];
        int val_free;
        int tmp_len;

        // If the hash key is an integer, convert it to a string
        if (mem) {
            //do nothing
        } else {
            tmp_len = snprintf(kbuf, sizeof(kbuf), "%ld", (long)idx);
            mem = zend_string_init((char*)kbuf, tmp_len, 0);
        }

        // Serialize value (if directed)
        val_free = redis_serialize(redis_sock, z_val, &val TSRMLS_CC);

        // Append the key and value to our command
        redis_cmd_append_sstr(&cmdstr, mem->val, mem->len);
        redis_cmd_append_sstr(&cmdstr, val->val, val->len);
    } ZEND_HASH_FOREACH_END();

    // Set slot if directed
    CMD_SET_SLOT(slot,key,key_len);

    // Free our key if we prefixed it
    if(key_free) efree(key);

    // Push return pointers
    *cmd_len = cmdstr.s->len;
    *cmd = cmdstr.s->val;

    // Success!
    return SUCCESS;
}

/* BITPOS */
int redis_bitpos_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    char *key;
    int argc, key_len, key_free;
    long bit, start, end;

    argc = ZEND_NUM_ARGS();
    if(zend_parse_parameters(argc TSRMLS_CC, "sl|ll", &key, &key_len, &bit,
                &start, &end)==FAILURE)
    {
        return FAILURE;
    }

    // Prevalidate bit
    if(bit != 0 && bit != 1) {
        return FAILURE;
    }

    // Prefix key
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Construct command based on arg count
    if(argc == 2) {
        *cmd_len = redis_cmd_format_static(cmd, "BITPOS", "sd", key, key_len,
                bit);
    } else if(argc == 3) {
        *cmd_len = redis_cmd_format_static(cmd, "BITPOS", "sdd", key, key_len,
                bit, start);
    } else {
        *cmd_len = redis_cmd_format_static(cmd, "BITPOS", "sddd", key, key_len,
                bit, start, end);
    }

    // Set our slot
    CMD_SET_SLOT(slot, key, key_len);

    return SUCCESS;
}

/* BITOP */
int redis_bitop_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    zval *z_args;
    char *key;
    int key_len, i, key_free, argc = ZEND_NUM_ARGS();
    smart_str cmdstr = {0};
    short kslot;

    // Allocate space for args, parse them as an array
    z_args = safe_emalloc(sizeof(zval), argc, 0);
    if(zend_get_parameters_array(ht, argc, z_args)==FAILURE ||
            argc < 3 || Z_TYPE(z_args[0]) != IS_STRING)
    {
        efree(z_args);
        return FAILURE;
    }

    // If we were passed a slot pointer, init to a sentinel value
    if(slot) *slot = -1;

    // Initialize command construction, add our operation argument
    redis_cmd_init_sstr(&cmdstr, argc, "BITOP", sizeof("BITOP")-1);
    redis_cmd_append_sstr(&cmdstr, Z_STRVAL(z_args[0]),
            Z_STRLEN(z_args[0]));

    // Now iterate over our keys argument
    for(i=1;i<argc;i++) {
        // Make sure we've got a string
        convert_to_string(&z_args[i]);

        // Grab this key and length
        key = Z_STRVAL(z_args[i]);
        key_len = Z_STRLEN(z_args[i]);

        // Prefix key, append
        key_free = redis_key_prefix(redis_sock, &key, &key_len);
        redis_cmd_append_sstr(&cmdstr, key, key_len);

        // Verify slot if this is a Cluster request
        if(slot) {
            kslot = cluster_hash_key(key, key_len);
            if(*slot == -1 || kslot != *slot) {
                php_error_docref(NULL TSRMLS_CC, E_WARNING,
                        "Warning, not all keys hash to the same slot!");
                if(key_free) efree(key);
                return FAILURE;
            }
            *slot = kslot;
        }

        if(key_free) efree(key);
    }

    // Free our argument array
    efree(z_args);

    // Push out variables
    *cmd = cmdstr.s->val;
    *cmd_len = cmdstr.s->len;

    return SUCCESS;
}

/* BITCOUNT */
int redis_bitcount_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    char *key;
    int key_len, key_free;
    long start = 0, end = -1;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|ll", &key, &key_len,
                &start, &end)==FAILURE)
    {
        return FAILURE;
    }

    // Prefix key, construct command
    key_free = redis_key_prefix(redis_sock, &key, &key_len);
    *cmd_len = redis_cmd_format_static(cmd, "BITCOUNT", "sdd", key, key_len,
            (int)start, (int)end);

    // Set our slot
    CMD_SET_SLOT(slot,key,key_len);

    // Fre key if we prefixed it
    if(key_free) efree(key);

    return SUCCESS;
}

/* PFADD and PFMERGE are the same except that in one case we serialize,
 * and in the other case we key prefix */
static int redis_gen_pf_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, int kw_len, int is_keys, char **cmd,
        int *cmd_len, short *slot)
{
    zval *z_arr, *z_ele;
    HashTable *ht_arr;
    smart_str cmdstr = {0};
    char *key;
    zend_string *mem;
    int key_len, key_free;
    int mem_free, argc=1;
    char *tmp_mem;
    int tmp_mem_len;

    // Parse arguments
    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa", &key, &key_len,
                &z_arr)==FAILURE)
    {
        return FAILURE;
    }

    // Grab HashTable, count total argc
    ht_arr = Z_ARRVAL_P(z_arr);
    argc += zend_hash_num_elements(ht_arr);

    // We need at least two arguments
    if(argc < 2) {
        return FAILURE;
    }

    // Prefix key, set initial hash slot
    key_free = redis_key_prefix(redis_sock, &key, &key_len);
    if(slot) *slot = cluster_hash_key(key, key_len);

    // Start command construction
    redis_cmd_init_sstr(&cmdstr, argc, kw, kw_len);
    redis_cmd_append_sstr(&cmdstr, key, key_len);

    // Free key if we prefixed
    if(key_free) efree(key);

    // Now iterate over the rest of our keys or values
    ZEND_HASH_FOREACH_VAL(ht_arr, z_ele) {
        zval z_tmp;


        // Prefix keys, serialize values
        if(is_keys) {
            if(Z_TYPE_P(z_ele)!=IS_STRING) {
                ZVAL_COPY(&z_tmp, z_ele);
                convert_to_string(&z_tmp);
                z_ele = &z_tmp;
            }

            tmp_mem = Z_STRVAL_P(z_ele);
            tmp_mem_len = Z_STRLEN_P(z_ele);

            // Key prefix
            mem_free = redis_key_prefix(redis_sock, &tmp_mem, &tmp_mem_len);

            // Verify slot
            if(slot && *slot != cluster_hash_key(tmp_mem, tmp_mem_len)) {
                php_error_docref(0 TSRMLS_CC, E_WARNING,
                        "All keys must hash to the same slot!");
                if(key_free) efree(key);
                if(&z_tmp) {
                    zval_dtor(&z_tmp);
                }
                return FAILURE;
            }
        } else {
            mem_free = redis_serialize(redis_sock, z_ele, &mem TSRMLS_CC);
            if(!mem_free) {
                if(Z_TYPE_P(z_ele)!=IS_STRING) {
                    ZVAL_COPY(&z_tmp, z_ele);
                    convert_to_string(&z_tmp);
                    z_ele = &z_tmp;
                }
                mem = zval_get_string(z_ele);
            }
        }

        // Append our key or member
        if(!is_keys) {
            redis_cmd_append_sstr(&cmdstr, mem->val, mem->len);
        } else {
            redis_cmd_append_sstr(&cmdstr, tmp_mem, tmp_mem_len);
        }

        // Clean up our temp val if it was used
        if(&z_tmp) {
            zval_dtor(&z_tmp);
        }

        // Clean up prefixed or serialized data

        if(mem_free) {
            if(!is_keys) {
                zend_string_free(mem);
            } else {
                efree(tmp_mem);
            }
        }
    } ZEND_HASH_FOREACH_END();

    // Push output arguments
    *cmd = cmdstr.s->val;
    *cmd_len = cmdstr.s->len;

    return SUCCESS;
}

/* PFADD */
int redis_pfadd_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    return redis_gen_pf_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
            "PFADD", sizeof("PFADD")-1, 0, cmd, cmd_len, slot);
}

/* PFMERGE */
int redis_pfmerge_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    return redis_gen_pf_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
            "PFMERGE", sizeof("PFMERGE")-1, 1, cmd, cmd_len, slot);
}

/* PFCOUNT */
int redis_pfcount_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
                      char **cmd, int *cmd_len, short *slot, void **ctx)
{
    zval *z_keys, *z_key, z_tmp;
    HashTable *ht_keys;
    HashPosition ptr;
    smart_str cmdstr = {0};
    int num_keys, key_len, key_free;
    char *key;
    short kslot=-1;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"z",&z_keys)==FAILURE) {
        return FAILURE;
    }

    /* If we were passed an array of keys, iterate through them prefixing if
     * required and capturing lengths and if we need to free them.  Otherwise
     * attempt to treat the argument as a string and just pass one */
    if (Z_TYPE_P(z_keys) == IS_ARRAY) {
        /* Grab key hash table and the number of keys */
        ht_keys = Z_ARRVAL_P(z_keys);
        num_keys = zend_hash_num_elements(ht_keys);

        /* There is no reason to send zero keys */
        if (num_keys == 0) {
            return FAILURE;
        }

        /* Initialize the command with our number of arguments */
        redis_cmd_init_sstr(&cmdstr, num_keys, "PFCOUNT", sizeof("PFCOUNT")-1);
        
        /* Append our key(s) */
        ZEND_HASH_FOREACH_VAL(ht_keys, z_key) {
            /* Turn our value into a string if it isn't one */
            if (Z_TYPE_P(z_key) != IS_STRING) {
                z_tmp = *z_key;
                zval_copy_ctor(&z_tmp);
                convert_to_string(&z_tmp);

                key = Z_STRVAL(z_tmp);
                key_len = Z_STRLEN(z_tmp);
            } else {
                key = Z_STRVAL_P(z_key);
                key_len = Z_STRLEN_P(z_key);
            }

            /* Append this key to our command */
            key_free = redis_key_prefix(redis_sock, &key, &key_len);
            redis_cmd_append_sstr(&cmdstr, key, key_len);
            
            /* Protect against CROSSLOT errors */
            if (slot) {
                if (kslot == -1) {
                    kslot = cluster_hash_key(key, key_len);
                } else if(cluster_hash_key(key,key_len)!=kslot) {
                    if (key_free) efree(key);
                    if (&z_tmp) {
                        zval_dtor(&z_tmp);
                    }
                    zend_string_release(cmdstr.s);
                    
                    php_error_docref(NULL TSRMLS_CC, E_WARNING,
                        "Not all keys hash to the same slot!");
                    return FAILURE;
                }
            }

            /* Cleanup */
            if (key_free) efree(key);
            if (&z_tmp) {
                zval_dtor(&z_tmp);
            }
        } ZEND_HASH_FOREACH_END();
    } else {
        /* Turn our key into a string if it's a different type */
        if (Z_TYPE_P(z_keys) != IS_STRING) {
            z_tmp = *z_keys;
            zval_copy_ctor(&z_tmp);
            convert_to_string(&z_tmp);

            key = Z_STRVAL(z_tmp);
            key_len = Z_STRLEN(z_tmp);
        } else {
            key = Z_STRVAL_P(z_keys);
            key_len = Z_STRLEN_P(z_keys);
        }

        /* Construct our whole command */
        redis_cmd_init_sstr(&cmdstr, 1, "PFCOUNT", sizeof("PFCOUNT")-1);
        key_free = redis_key_prefix(redis_sock, &key, &key_len);
        redis_cmd_append_sstr(&cmdstr, key, key_len);

        /* Hash our key */
        CMD_SET_SLOT(slot, key, key_len);

        /* Cleanup */
        if (key_free) efree(key);
        if (&z_tmp) {
            zval_dtor(&z_tmp);
        }
    }

    /* Push our command and length to the caller */
    *cmd = cmdstr.s->val;
    *cmd_len = cmdstr.s->len;

    return SUCCESS;
}

int redis_auth_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    char *pw;
    int pw_len;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &pw, &pw_len)
            ==FAILURE)
    {
        return FAILURE;
    }

    // Construct our AUTH command
    *cmd_len = redis_cmd_format_static(cmd, "AUTH", "s", pw, pw_len);

    // Free previously allocated password, and update
    if(redis_sock->auth) efree(redis_sock->auth);
    redis_sock->auth = estrndup(pw, pw_len);

    // Success
    return SUCCESS;
}

/* SETBIT */
int redis_setbit_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    char *key;
    int key_free;
    size_t key_len;
    long long offset;
    zend_bool val;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slb", &key, &key_len,
                &offset, &val)==FAILURE)
    {
        return FAILURE;
    }

    // Validate our offset
    if(offset < BITOP_MIN_OFFSET || offset > BITOP_MAX_OFFSET) {
        php_error_docref(0 TSRMLS_CC, E_WARNING,
                "Invalid OFFSET for bitop command (must be between 0-2^32-1)");
        return FAILURE;
    }

    key_free = redis_key_prefix(redis_sock, &key, &key_len);
    *cmd_len = redis_cmd_format_static(cmd, "SETBIT", "sld", key, key_len,
        offset, (int)val);

    CMD_SET_SLOT(slot, key, key_len);

    if(key_free) efree(key);

    return SUCCESS;
}

/* LINSERT */
int redis_linsert_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    char *key, *pos;
    size_t key_len, pos_len;
    int key_free, pivot_free, val_free;
    zval *z_val, *z_pivot;
    zend_string *val, *pivot;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sszz", &key, &key_len,
                &pos, &pos_len, &z_pivot, &z_val)==FAILURE)
    {
        return FAILURE;
    }

    // Validate position
    if(strncasecmp(pos, "after", 5) && strncasecmp(pos, "before", 6)) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING,
                "Position must be either 'BEFORE' or 'AFTER'");
        return FAILURE;
    }

    // Prefix key, serialize value and position
    key_free = redis_key_prefix(redis_sock, &key, &key_len);
    val_free = redis_serialize(redis_sock, z_val, &val TSRMLS_CC);
    pivot_free = redis_serialize(redis_sock, z_pivot, &pivot TSRMLS_CC);

    // Construct command
    *cmd_len = redis_cmd_format_static(cmd, "LINSERT", "ssss", key, key_len,
            pos, pos_len, pivot->val, pivot->len, val->val, val->len);

    // Set slot
    CMD_SET_SLOT(slot, key, key_len);

    // Clean up
    if(val_free) zend_string_free(val);
    if(key_free) efree(key);
    if(pivot_free) zend_string_free(pivot);

    // Success
    return SUCCESS;
}

/* LREM */
int redis_lrem_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    char *key;
    int key_free, val_free;
    size_t key_len;
    long count = 0;
    zval *z_val;
    zend_string *val;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz|l", &key, &key_len,
                &z_val, &count)==FAILURE)
    {
        return FAILURE;
    }

    // Prefix key, serialize value
    key_free = redis_key_prefix(redis_sock, &key, &key_len);
    val_free = redis_serialize(redis_sock, z_val, &val TSRMLS_CC);

    // Construct command
    *cmd_len = redis_cmd_format_static(cmd, "LREM", "sds", key, key_len, count,
            val->val, val->len);

    // Set slot
    CMD_SET_SLOT(slot, key, key_len);

    // Cleanup
    if(val_free) zend_string_free(val);
    if(key_free) efree(key);

    // Success!
    return SUCCESS;
}

int redis_smove_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    char *src, *dst;
    size_t src_len, dst_len;
    int val_free, src_free, dst_free;
    zval *z_val;
    zend_string *val;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssz", &src, &src_len,
                &dst, &dst_len, &z_val)==FAILURE)
    {
        return FAILURE;
    }

    val_free = redis_serialize(redis_sock, z_val, &val TSRMLS_CC);
    src_free = redis_key_prefix(redis_sock, &src, &src_len);
    dst_free = redis_key_prefix(redis_sock, &dst, &dst_len);

    // Protect against a CROSSSLOT error
    if(slot) {
        short slot1 = cluster_hash_key(src, src_len);
        short slot2 = cluster_hash_key(dst, dst_len);
        if(slot1 != slot2) {
            php_error_docref(0 TSRMLS_CC, E_WARNING,
                    "Source and destination keys don't hash to the same slot!");
            if(val_free) zend_string_free(val);
            if(src_free) efree(src);
            if(dst_free) efree(dst);
            return FAILURE;
        }
        *slot = slot1;
    }

    // Construct command
    *cmd_len = redis_cmd_format_static(cmd, "SMOVE", "sss", src, src_len, dst,
            dst_len, val->val, val->len);

    // Cleanup
    if(val_free) zend_string_free(val);
    if(src_free) efree(src);
    if(dst_free) efree(dst);

    // Succcess!
    return SUCCESS;
}

/* Generic command construction for HSET and HSETNX */
static int gen_hset_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char *kw, char **cmd, int *cmd_len, short *slot)
{
    char *key, *mem;
    size_t key_len, mem_len;
    int val_free, key_free;
    zval *z_val;
    zend_string *val;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssz", &key, &key_len,
                &mem, &mem_len, &z_val)==FAILURE)
    {
        return FAILURE;
    }

    // Prefix/serialize
    val_free = redis_serialize(redis_sock, z_val, &val TSRMLS_CC);
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Construct command
    *cmd_len = redis_cmd_format_static(cmd, kw, "sss", key, key_len, mem,
            mem_len, val->val, val->len);

    // Set slot
    CMD_SET_SLOT(slot,key,key_len);

    /* Cleanup our key and value */
    if (val_free) zend_string_free(val);
    if (key_free) efree(key);

    // Success
    return SUCCESS;
}

/* HSET */
int redis_hset_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    return gen_hset_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, "HSET",
            cmd, cmd_len, slot);
}

/* HSETNX */
int redis_hsetnx_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    return gen_hset_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, "HSETNX",
            cmd, cmd_len, slot);
}

/* SRANDMEMBER */
int redis_srandmember_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx,
        short *have_count)
{
    char *key;
    int key_free;
    size_t key_len;
    long long count;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &key, &key_len,
                &count)==FAILURE)
    {
        return FAILURE;
    }

    // Prefix key if requested
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Set our have count flag
    *have_count = ZEND_NUM_ARGS() == 2;

    // Two args means we have the optional COUNT
    if(*have_count) {
        *cmd_len = redis_cmd_format_static(cmd, "SRANDMEMBER", "sl", key,
                key_len, count);
    } else {
        *cmd_len = redis_cmd_format_static(cmd, "SRANDMEMBER", "s", key,
                key_len);
    }

    // Set slot
    CMD_SET_SLOT(slot,key,key_len);

    // Cleanup
    if(key_free) efree(key);

    return SUCCESS;
}

/* ZINCRBY */
int redis_zincrby_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    char *key;
    size_t key_len;
    int key_free, mem_free;
    double incrby;
    zval *z_val;
    zend_string *mem;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sdz", &key, &key_len,
                &incrby, &z_val)==FAILURE)
    {
        return FAILURE;
    }

    // Prefix key, serialize
    key_free = redis_key_prefix(redis_sock, &key, &key_len);
    mem_free = redis_serialize(redis_sock, z_val, &mem TSRMLS_CC);

    *cmd_len = redis_cmd_format_static(cmd, "ZINCRBY", "sfs", key, key_len,
            incrby, mem->val, mem->len);

    CMD_SET_SLOT(slot,key,key_len);

    // Cleanup
    if(key_free) efree(key);
    if(mem_free) zend_string_free(mem);

    return SUCCESS;
}

/* SORT */
int redis_sort_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        int *using_store, char **cmd, int *cmd_len, short *slot,
        void **ctx)
{
    zval *z_opts=NULL, *z_ele, z_argv;
    char *key;
    HashTable *ht_opts;
    smart_str cmdstr = {0};
    int key_len, key_free;
	long low, high;
	HashTable *ht_argv;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|a", &key, &key_len,
                &z_opts)==FAILURE)
    {
        return FAILURE;
    }

    // Default that we're not using store
    *using_store = 0;

    // Handle key prefixing
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // If we don't have an options array, the command is quite simple
    if(!z_opts || zend_hash_num_elements(Z_ARRVAL_P(z_opts)) == 0) {
        // Construct command
        *cmd_len = redis_cmd_format_static(cmd, "SORT", "s", key, key_len);

        // Push out slot, store flag, and clean up
        *using_store = 0;
        CMD_SET_SLOT(slot,key,key_len);
        if(key_free) efree(key);

        return SUCCESS;
    }

    // Create our hash table to hold our sort arguments
    array_init(&z_argv);

    // SORT <key>
    add_next_index_stringl(&z_argv, key, key_len);

    // Set slot
    CMD_SET_SLOT(slot,key,key_len);

    // Grab the hash table
    ht_opts = Z_ARRVAL_P(z_opts);

    // Handle BY pattern
    if (((z_ele = zend_hash_str_find(ht_opts, "by", sizeof("by") - 1)) != NULL ||
                (z_ele = zend_hash_str_find(ht_opts, "BY", sizeof("BY") - 1)) != NULL) &&
            Z_TYPE_P(z_ele)==IS_STRING) {
        // "BY" option is disabled in cluster
        if(slot) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING,
                    "SORT BY option is not allowed in Redis Cluster");
            if(key_free) efree(key);
            zval_dtor(&z_argv);
            efree(&z_argv);
            return FAILURE;
        }

        // ... BY <pattern>
        add_next_index_stringl(&z_argv, "BY", sizeof("BY")-1);
        add_next_index_stringl(&z_argv,Z_STRVAL_P(z_ele),Z_STRLEN_P(z_ele));
    }

    // Handle ASC/DESC option
    if (((z_ele = zend_hash_str_find(ht_opts, "sort", sizeof("sort") - 1)) != NULL ||
                (z_ele = zend_hash_str_find(ht_opts, "SORT", sizeof("SORT") - 1)) != NULL) &&
            Z_TYPE_P(z_ele)==IS_STRING) {
        // 'asc'|'desc'
        add_next_index_stringl(&z_argv,Z_STRVAL_P(z_ele),Z_STRLEN_P(z_ele));
    }

    // STORE option
    if (((z_ele = zend_hash_str_find(ht_opts, "store", 5)) != NULL ||
                (z_ele = zend_hash_str_find(ht_opts, "STORE", 5)) != NULL) &&
            Z_TYPE_P(z_ele)==IS_STRING) {
        // Slot verification
        int cross_slot = slot && *slot != cluster_hash_key(
                Z_STRVAL_P(z_ele),Z_STRLEN_P(z_ele));

        if(cross_slot) {
            php_error_docref(0 TSRMLS_CC, E_WARNING,
                    "Error, SORT key and STORE key have different slots!");
            if(key_free) efree(key);
            zval_dtor(&z_argv);
            efree(&z_argv);
            return FAILURE;
        }

        // STORE <key>
        add_next_index_stringl(&z_argv,"STORE",sizeof("STORE")-1);
        add_next_index_stringl(&z_argv,Z_STRVAL_P(z_ele),Z_STRLEN_P(z_ele));

        // We are using STORE
        *using_store = 1;
    }

    // GET option
    if (((z_ele = zend_hash_str_find(ht_opts, "get", 3)) != NULL ||
                (z_ele = zend_hash_str_find(ht_opts, "GET", 3)) != NULL) &&
            (Z_TYPE_P(z_ele)==IS_STRING || Z_TYPE_P(z_ele)==IS_ARRAY)) {

        // Disabled in cluster
        if(slot) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING,
                    "GET option for SORT disabled in Redis Cluster");
            if(key_free) efree(key);
            zval_dtor(&z_argv);
            efree(&z_argv);
            return FAILURE;
        }

        // If it's a string just add it
        if(Z_TYPE_P(z_ele)==IS_STRING) {
            add_next_index_stringl(&z_argv,"GET",sizeof("GET")-1);
            add_next_index_stringl(&z_argv,Z_STRVAL_P(z_ele),
                    Z_STRLEN_P(z_ele));
        } else {
            HashTable *ht_keys = Z_ARRVAL_P(z_ele);
            int added=0;
            zval *z_key;

            ZEND_HASH_FOREACH_VAL(ht_keys, z_key) {
                if (Z_TYPE_P(z_key)!=IS_STRING) {
                    continue;
                }

                /* Add get per thing we're getting */
                add_next_index_stringl(&z_argv,"GET",sizeof("GET")-1);

                // Add this key to our argv array
                add_next_index_stringl(&z_argv, Z_STRVAL_P(z_key),
                        Z_STRLEN_P(z_key));
                added++;

            } ZEND_HASH_FOREACH_END();

            // Make sure we were able to add at least one
            if(added==0) {
                php_error_docref(NULL TSRMLS_CC, E_WARNING,
                        "Array of GET values requested, but none are valid");
                if(key_free) efree(key);
                zval_dtor(&z_argv);
                efree(&z_argv);
                return FAILURE;
            }
        }
    }

    // ALPHA
    if (((z_ele = zend_hash_str_find(ht_opts, "alpha", 5)) != NULL ||
                (z_ele = zend_hash_str_find(ht_opts, "ALPHA", 5)) != NULL) &&
            Z_TYPE_P(z_ele)==IS_TRUE) {
        add_next_index_stringl(&z_argv, "ALPHA", sizeof("ALPHA")-1);
    }

    // LIMIT <offset> <count>
    if (((z_ele = zend_hash_str_find(ht_opts, "limit", 5)) != NULL ||
                (z_ele = zend_hash_str_find(ht_opts, "LIMIT", 5)) != NULL) &&
            Z_TYPE_P(z_ele)==IS_ARRAY) {

        HashTable *ht_off = Z_ARRVAL_P(z_ele);
        zval *z_off, *z_cnt;

        if ((z_off = zend_hash_index_find(ht_off, 0)) != NULL &&
                (z_cnt = zend_hash_index_find(ht_off, 1)) != NULL) {

            if((Z_TYPE_P(z_off)!=IS_STRING && Z_TYPE_P(z_off)!=IS_LONG) ||
                    (Z_TYPE_P(z_cnt)!=IS_STRING && Z_TYPE_P(z_cnt)!=IS_LONG))
            {
                php_error_docref(NULL TSRMLS_CC, E_WARNING,
                        "LIMIT options on SORT command must be longs or strings");
                if(key_free) efree(key);
                zval_dtor(&z_argv);
                efree(&z_argv);
                return FAILURE;
            }

            // Add LIMIT argument
            add_next_index_stringl(&z_argv,"LIMIT",sizeof("LIMIT")-1);

            
            if(Z_TYPE_P(z_off)==IS_STRING) {
                low = atol(Z_STRVAL_P(z_off));
            } else {
                low = Z_LVAL_P(z_off);
            }
            if(Z_TYPE_P(z_cnt)==IS_STRING) {
                high = atol(Z_STRVAL_P(z_cnt));
            } else {
                high = Z_LVAL_P(z_cnt);
            }

            // Add our two LIMIT arguments
            add_next_index_long(&z_argv, low);
            add_next_index_long(&z_argv, high);
        }
    }

    // Start constructing our command
    ht_argv = Z_ARRVAL(z_argv);
    redis_cmd_init_sstr(&cmdstr, zend_hash_num_elements(ht_argv), "SORT",
            sizeof("SORT")-1);

    // Iterate through our arguments
    ZEND_HASH_FOREACH_VAL(ht_argv, z_ele) {
        // Args are strings or longs
        if(Z_TYPE_P(z_ele)==IS_STRING) {
            redis_cmd_append_sstr(&cmdstr,Z_STRVAL_P(z_ele),
                    Z_STRLEN_P(z_ele));
        } else {
            redis_cmd_append_sstr_long(&cmdstr, Z_LVAL_P(z_ele));
        }

    } ZEND_HASH_FOREACH_END();

    /* Clean up our arguments array.  Note we don't have to free any prefixed
     * key as that we didn't duplicate the pointer if we prefixed */
    zval_dtor(&z_argv);

    // Push our length and command
    *cmd_len = cmdstr.s->len;
    *cmd     = cmdstr.s->val;

    // Success!
    return SUCCESS;
}

/* HDEL */
int redis_hdel_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    zval *z_args;
    smart_str cmdstr = {0};
    char *arg;
    int arg_free, arg_len, i;
    int argc = ZEND_NUM_ARGS();

    // We need at least KEY and one member
    if(argc < 2) {
        return FAILURE;
    }

    // Grab arguments as an array
    z_args = safe_emalloc(sizeof(zval), argc, 0);
    if(zend_get_parameters_array(ht, argc, z_args)==FAILURE) {
        efree(z_args);
        return FAILURE;
    }

    // Get first argument (the key) as a string
    convert_to_string(&z_args[0]);
    arg = Z_STRVAL(z_args[0]);
    arg_len = Z_STRLEN(z_args[0]);

    // Prefix
    arg_free = redis_key_prefix(redis_sock, &arg, &arg_len);

    // Start command construction
    redis_cmd_init_sstr(&cmdstr, argc, "HDEL", sizeof("HDEL")-1);
    redis_cmd_append_sstr(&cmdstr, arg, arg_len);

    // Set our slot, free key if we prefixed it
    CMD_SET_SLOT(slot,arg,arg_len);
    if(arg_free) efree(arg);

    // Iterate through the members we're removing
    for(i=1;i<argc;i++) {
        convert_to_string(&z_args[i]);
        redis_cmd_append_sstr(&cmdstr, Z_STRVAL(z_args[i]),
                Z_STRLEN(z_args[i]));
    }

    // Push out values
    *cmd     = cmdstr.s->val;
    *cmd_len = cmdstr.s->len;

    // Cleanup
    efree(z_args);

    // Success!
    return SUCCESS;
}

/* ZADD */
int redis_zadd_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    zval *z_args;
    char *key;
    int key_len, key_free, val_free;
    int argc = ZEND_NUM_ARGS(), i;
    smart_str cmdstr = {0};
    zend_string *val;

    z_args = safe_emalloc(sizeof(zval), argc, 0);
    if(zend_get_parameters_array(ht, argc, z_args)==FAILURE) {
        efree(z_args);
        return FAILURE;
    }

    // Need key, score, value, [score, value...] */
    if(argc>0) convert_to_string(&z_args[0]);
    if(argc<3 || Z_TYPE(z_args[0])!=IS_STRING || (argc-1)%2 != 0) {
        efree(z_args);
        return FAILURE;
    }

    // Prefix our key
    key = Z_STRVAL(z_args[0]);
    key_len = Z_STRLEN(z_args[0]);
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Start command construction
    redis_cmd_init_sstr(&cmdstr, argc, "ZADD", sizeof("ZADD")-1);
    redis_cmd_append_sstr(&cmdstr, key, key_len);

    // Set our slot, free key if we prefixed it
    CMD_SET_SLOT(slot,key,key_len);
    if(key_free) efree(key);

    // Now the rest of our arguments
    for(i=1;i<argc;i+=2) {
        // Convert score to a double, serialize value if requested
        convert_to_double(&z_args[i]);
        val_free = redis_serialize(redis_sock, &z_args[i+1], &val TSRMLS_CC);

        // Append score and member
        redis_cmd_append_sstr_dbl(&cmdstr, Z_DVAL(z_args[i]));
        redis_cmd_append_sstr(&cmdstr, val->val, val->len);

        // Free value if we serialized
        if(val_free) zend_string_free(val);
    }

    // Push output values
    *cmd     = cmdstr.s->val;
    *cmd_len = cmdstr.s->len;

    // Cleanup args
    efree(z_args);

    return SUCCESS;
}

/* OBJECT */
int redis_object_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        REDIS_REPLY_TYPE *rtype, char **cmd, int *cmd_len,
        short *slot, void **ctx)
{
    char *key, *subcmd;
    int key_len, key_free, subcmd_len;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &subcmd,
                &subcmd_len, &key, &key_len)==FAILURE)
    {
        return FAILURE;
    }

    // Prefix our key
    key_free = redis_key_prefix(redis_sock, &key, &key_len);

    // Format our command
    *cmd_len = redis_cmd_format_static(cmd, "OBJECT", "ss", subcmd, subcmd_len,
            key, key_len);

    // Set our slot, free key if we prefixed
    CMD_SET_SLOT(slot,key,key_len);
    if(key_free) efree(key);

    // Push the reply type to our caller
    if(subcmd_len == 8 && (!strncasecmp(subcmd,"refcount",8) ||
                !strncasecmp(subcmd,"idletime",8)))
    {
        *rtype = TYPE_INT;
    } else if(subcmd_len == 8 && !strncasecmp(subcmd, "encoding", 8)) {
        *rtype = TYPE_BULK;
    } else {
        php_error_docref(NULL TSRMLS_CC, E_WARNING,
                "Invalid subcommand sent to OBJECT");
        efree(*cmd);
        return FAILURE;
    }

    // Success
    return SUCCESS;
}

/* DEL */
int redis_del_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    return gen_varkey_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
            "DEL", sizeof("DEL")-1, 1, 0, cmd, cmd_len, slot);
}

/* WATCH */
int redis_watch_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    return gen_varkey_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
            "WATCH", sizeof("WATCH")-1, 1, 0, cmd, cmd_len, slot);
}

/* BLPOP */
int redis_blpop_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    return gen_varkey_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
            "BLPOP", sizeof("BLPOP")-1, 2, 1, cmd, cmd_len, slot);
}

/* BRPOP */
int redis_brpop_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    return gen_varkey_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
            "BRPOP", sizeof("BRPOP")-1, 1, 1, cmd, cmd_len, slot);
}

/* SINTER */
int redis_sinter_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    return gen_varkey_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
            "SINTER", sizeof("SINTER")-1, 1, 0, cmd, cmd_len, slot);
}

/* SINTERSTORE */
int redis_sinterstore_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    return gen_varkey_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
            "SINTERSTORE", sizeof("SINTERSTORE")-1, 2, 0, cmd, cmd_len, slot);
}

/* SUNION */
int redis_sunion_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    return gen_varkey_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
            "SUNION", sizeof("SUNION")-1, 1, 0, cmd, cmd_len, slot);
}

/* SUNIONSTORE */
int redis_sunionstore_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    return gen_varkey_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
            "SUNIONSTORE", sizeof("SUNIONSTORE")-1, 2, 0, cmd, cmd_len, slot);
}

/* SDIFF */
int redis_sdiff_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    return gen_varkey_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, "SDIFF",
            sizeof("SDIFF")-1, 1, 0, cmd, cmd_len, slot);
}

/* SDIFFSTORE */
int redis_sdiffstore_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
        char **cmd, int *cmd_len, short *slot, void **ctx)
{
    return gen_varkey_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
            "SDIFFSTORE", sizeof("SDIFFSTORE")-1, 2, 0, cmd, cmd_len, slot);
}

/* COMMAND */
int redis_rawcommand_cmd(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock,
                         char **cmd, int *cmd_len, short *slot, void **ctx)
{
    char *kw=NULL;
    zval *z_arg;
    int kw_len;
	zval *z_ele;
	HashTable *ht_arr;
	//smart_str cmdstr;
	smart_str cmdstr = {0};

    /* Parse our args */
    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sz", &kw, &kw_len, 
                &z_arg)==FAILURE) 
    {
        return FAILURE;
    }

    /* Construct our command */
    if(!kw) {
        *cmd_len = redis_cmd_format_static(cmd, "COMMAND", "");
    } else if(kw && !z_arg) {
        /* Sanity check */
        if(strncasecmp(kw, "info", sizeof("info")-1) || 
                Z_TYPE_P(z_arg)!=IS_STRING)
        {
            return FAILURE;
        }

        /* COMMAND INFO <cmd> */
        *cmd_len = redis_cmd_format_static(cmd, "COMMAND", "ss", "INFO",
                sizeof("INFO")-1, Z_STRVAL_P(z_arg), Z_STRLEN_P(z_arg));
    } else {
        int arr_len;

        /* Sanity check on args */
        if(strncasecmp(kw, "getkeys", sizeof("getkeys")-1) ||
                Z_TYPE_P(z_arg)!=IS_ARRAY || 
                (arr_len=zend_hash_num_elements(Z_ARRVAL_P(z_arg)))<1)
        {
            return FAILURE;
        }

        
        ht_arr = Z_ARRVAL_P(z_arg);
        

        redis_cmd_init_sstr(&cmdstr, 1 + arr_len, "COMMAND", sizeof("COMMAND")-1);
        redis_cmd_append_sstr(&cmdstr, "GETKEYS", sizeof("GETKEYS")-1);

        ZEND_HASH_FOREACH_VAL(ht_arr, z_ele) {
            convert_to_string(z_ele);
            redis_cmd_append_sstr(&cmdstr, Z_STRVAL_P(z_ele), Z_STRLEN_P(z_ele));
        } ZEND_HASH_FOREACH_END();

        *cmd = cmdstr.s->val;
        *cmd_len = cmdstr.s->len;
    }

    /* Any slot will do */
    CMD_RAND_SLOT(slot);

    return SUCCESS;
}

/*
 * Redis commands that don't deal with the server at all.  The RedisSock*
 * pointer is the only thing retreived differently, so we just take that
 * in additon to the standard INTERNAL_FUNCTION_PARAMETERS for arg parsing,
 * return value handling, and thread safety. */

void redis_getoption_handler(INTERNAL_FUNCTION_PARAMETERS,
        RedisSock *redis_sock, redisCluster *c)
{
    long option;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &option)
            == FAILURE)
    {
        RETURN_FALSE;
    }

    // Return the requested option
    switch(option) {
        case REDIS_OPT_SERIALIZER:
            RETURN_LONG(redis_sock->serializer);
        case REDIS_OPT_PREFIX:
            if(redis_sock->prefix) {
                RETURN_STRINGL(redis_sock->prefix, redis_sock->prefix_len);
            }
            RETURN_NULL();
        case REDIS_OPT_READ_TIMEOUT:
            RETURN_DOUBLE(redis_sock->read_timeout);
        case REDIS_OPT_SCAN:
            RETURN_LONG(redis_sock->scan);
        case REDIS_OPT_FAILOVER:
            RETURN_LONG(c->failover);
        default:
            RETURN_FALSE;
    }
}

void redis_setoption_handler(INTERNAL_FUNCTION_PARAMETERS,
        RedisSock *redis_sock, redisCluster *c)
{
    long option, val_long;
    char *val_str;
    int val_len;
    struct timeval read_tv;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ls", &option,
                &val_str, &val_len) == FAILURE)
    {
        RETURN_FALSE;
    }

    switch(option) {
        case REDIS_OPT_SERIALIZER:
            val_long = atol(val_str);
            if(val_long == REDIS_SERIALIZER_NONE
#ifdef HAVE_REDIS_IGBINARY
                    || val_long == REDIS_SERIALIZER_IGBINARY
#endif
                    || val_long == REDIS_SERIALIZER_PHP)
            {
                redis_sock->serializer = val_long;
                RETURN_TRUE;
            } else {
                RETURN_FALSE;
            }
            break;
        case REDIS_OPT_PREFIX:
            if(redis_sock->prefix) {
                efree(redis_sock->prefix);
            }
            if(val_len == 0) {
                redis_sock->prefix = NULL;
                redis_sock->prefix_len = 0;
            } else {
                redis_sock->prefix_len = val_len;
                redis_sock->prefix = ecalloc(1+val_len, 1);
                memcpy(redis_sock->prefix, val_str, val_len);
            }
            RETURN_TRUE;
        case REDIS_OPT_READ_TIMEOUT:
            redis_sock->read_timeout = atof(val_str);
            if(redis_sock->stream) {
                read_tv.tv_sec  = (time_t)redis_sock->read_timeout;
                read_tv.tv_usec = (int)((redis_sock->read_timeout -
                            read_tv.tv_sec) * 1000000);
                php_stream_set_option(redis_sock->stream,
                        PHP_STREAM_OPTION_READ_TIMEOUT, 0,
                        &read_tv);
            }
            RETURN_TRUE;
        case REDIS_OPT_SCAN:
            val_long = atol(val_str);
            if(val_long==REDIS_SCAN_NORETRY || val_long==REDIS_SCAN_RETRY) {
                redis_sock->scan = val_long;
                RETURN_TRUE;
            }
            RETURN_FALSE;
            break;
        case REDIS_OPT_FAILOVER:
            val_long = atol(val_str);
            if (val_long == REDIS_FAILOVER_NONE || 
                val_long == REDIS_FAILOVER_ERROR ||
                val_long == REDIS_FAILOVER_DISTRIBUTE)
            {
                c->failover = val_long;
                RETURN_TRUE;
            } else {
                RETURN_FALSE;
            } 
        default:
            RETURN_FALSE;
    }
}

void redis_prefix_handler(INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock) {
    char *key;
    int key_len;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len)
            ==FAILURE)
    {
        RETURN_FALSE;
    }

    if(redis_sock->prefix != NULL && redis_sock->prefix_len>0) {
        redis_key_prefix(redis_sock, &key, &key_len);
        RETURN_STRINGL(key, key_len);
    } else {
        RETURN_STRINGL(key, key_len);
    }
}

void redis_serialize_handler(INTERNAL_FUNCTION_PARAMETERS,
        RedisSock *redis_sock)
{
    zval *z_val;
    zend_string *val;
	int val_free;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &z_val)==FAILURE) {
        RETURN_FALSE;
    }

    val_free = redis_serialize(redis_sock, z_val, &val TSRMLS_CC);

    RETVAL_STRINGL(val->val, val->len);
    if(val_free) zend_string_free(val);
}

void redis_unserialize_handler(INTERNAL_FUNCTION_PARAMETERS,
        RedisSock *redis_sock, zend_class_entry *ex)
{
    char *value;
    int value_len;

    // Parse our arguments
    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &value, &value_len)
            == FAILURE)
    {
        RETURN_FALSE;
    }

    // We only need to attempt unserialization if we have a serializer running
    if(redis_sock->serializer != REDIS_SERIALIZER_NONE) {
        zval *z_ret = NULL;
        if(redis_unserialize(redis_sock, value, value_len, &z_ret
                    TSRMLS_CC) == 0)
        {
            // Badly formed input, throw an execption
            zend_throw_exception(ex,
                    "Invalid serialized data, or unserialization error",
                    0 TSRMLS_CC);
            RETURN_FALSE;
        }
        RETURN_ZVAL(z_ret, 0, 1);
    } else {
        // Just return the value that was passed to us
        RETURN_STRINGL(value, value_len);
    }
}

/* vim: set tabstop=4 softtabstops=4 noexpandtab shiftwidth=4: */
