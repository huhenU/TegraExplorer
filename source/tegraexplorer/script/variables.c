#include <string.h>
#include "../../mem/heap.h"
#include "../gfx/gfxutils.h"
#include "../emmc/emmc.h"
#include "../../utils/types.h"
#include "../../libs/fatfs/ff.h"
#include "../../utils/sprintf.h"
#include "../../utils/btn.h"
#include "../../gfx/gfx.h"
#include "../../utils/util.h"
#include "../../storage/emummc.h"
#include "parser.h"
#include "../common/common.h"
#include "../fs/fsactions.h"
#include "variables.h"
#include "../utils/utils.h"

static dict_str_void *str_int_table = NULL;
static dict_str_void *str_str_table = NULL;
//static dict_str_loc *str_jmp_table = NULL;

int str_void_add(char *key, void* value, dict_str_void **in){
    char *key_local;
    dict_str_void *entry, *iter = *in;

    utils_copystring(key, &key_local);
    entry = calloc(1, sizeof(dict_str_void));

    entry->key = key_local;
    entry->value = value;
    entry->next = NULL;
    
    if (iter == NULL){
        *in = entry;
        return 0;
    }
    else {
        while (iter != NULL){
            if (!strcmp(iter->key, key_local)){
                free(entry);
                free(key_local);
                free(iter->value);
                iter->value = value;
                return 0;
            }

            if (iter->next == NULL){
                iter->next = entry;
                return 0;
            }

            iter = iter->next;
        }
        return -1;
    }
}

int str_void_find(char *key, void **out, dict_str_void *in){
    while (in != NULL){
        if (!strcmp(in->key, key)){
            *out = in->value;
            return 0;
        }
        in = in->next;
    }

    return -1;
}

void *str_void_index(int index, dict_str_void *in){
    dict_str_void *iter = in;

    for (int i = 0; i < index && iter != NULL; i++){
       iter = iter->next;
    }

    if (iter == NULL)
        return NULL;

    return iter->value;
}

void str_void_clear(dict_str_void **in){
    dict_str_void *iter = *in, *next;

    while (iter != NULL){
        next = iter->next;
        free(iter->key);
        free(iter->value);
        free(iter);
        iter = next;
    }
    
    *in = NULL;
}


int str_int_add(char *key, int value){
    int *item = malloc(sizeof(int));
    *item = value;

    return str_void_add(key, item, &str_int_table);
}

int str_int_find(char *key, int *out){
   int *item = NULL;
   int res = str_void_find(key, (void**)&item, str_int_table);
   *out = *item;
   return res;
}

void str_int_clear(){
   str_void_clear(&str_int_table);
}


int str_str_add(char *key, char *value){
    char *value_cpy;
    utils_copystring(value, &value_cpy);

    return str_void_add(key, value_cpy, &str_str_table);
}

int str_str_find(char *key, char **out){
    return str_void_find(key, (void**)out, str_str_table);
}

int str_str_index(int index, char **out){
    char *ret;
    ret = (char *)str_void_index(index, str_str_table);

    if (ret == NULL)
        return -1;
    
    *out = ret;
    return 0;
}

void str_str_clear(){
    str_void_clear(&str_str_table);
}