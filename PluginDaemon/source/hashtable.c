#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <syslog.h>
#include "hashtable.h"
#include "misc.h"

//number of probes to perform
#define LOOKUP_ATTEMPTS 5

//how large to grow the table each time it is
//automatically resized. As a percentage of its
//current size.
#define GROWTH_RATE 0.21f


static size_t growSize(size_t oldSize) {

  return oldSize + (size_t) floor(oldSize * GROWTH_RATE);
}


HashData_t *HashData_create(char *key, char *data) {

  HashData_t *entry = calloc(1, sizeof(HashData_t));
  if (!entry) {
    SYSLOG(LOG_ERR, "HashData_create: Failed to allocate entry");
    return NULL;
  }

  //set the key to whatever the token was pointing at
  entry->key = calloc(1, strlen(key) + 1);
  if (!entry->key) {
    SYSLOG(LOG_ERR, "HashData_create: Failed to allocate key: %s", key);
    free(entry);
    return NULL;
  }
  strcpy(entry->key, key);

  //copy the value
  entry->value = calloc(1, strlen(data) + 1);
  if (!entry->key) {
    SYSLOG(LOG_ERR, "HashData_create: Failed to allocate data for: %s: %s", key, data);
    free(entry->key);
    free(entry);
    return NULL;
  }
  strcpy(entry->value, data);
  return entry;
}

void HashData_destroy(HashData_t *data) {

  if (!data)
    return;

  if (data->key)
    free(data->key);
  if (data->value)
    free(data->value);

  memset(data, 0, sizeof(HashData_t));
  free(data);
}


void HashData_print(FILE *output, HashData_t *entry) {

  if (!entry || !entry->key)
    return;

  fprintf(output, "%s:%s\n", entry->key, entry->value);
}


//djb2 algorithm
//http://www.cse.yorku.ca/~oz/hash.html
size_t hash1(char *key, size_t num, size_t last) {

  size_t hashVal = 5381 + last;
  int c;

  while ((c = *key++) != '\0')
    hashVal = ((hashVal << 5) + hashVal) ^ c; /* hash * 33 + c */


  return hashVal % num;
}

size_t hash2(char *key, size_t num, int attempt1) {

  size_t hashVal = 0;
  while (*key != '\0') {
    hashVal = ((hashVal << 4) + *key) % num;
    key++;
  }

  return (attempt1 - hashVal) % num;
}


HashData_t **HashTable_getEntry(HashTable_t *table, char *key) {

  if (!table || !key)
    return NULL;

  size_t pos = hash1(key, table->size, 0);

  HashData_t **curPos = &table->entries[pos];
  if (!curPos) {
    SYSLOG(LOG_ERR, "HashTable_getEntry: No table entries allocated?");
    //no table entries allocated?
    return NULL;
  }
  //if first attempt and nothing exists, return the free position
  if (!*curPos) {
    return curPos;
  }

  //otherwise, check that we've got the right one
  int attempt = 0;

  do {
    HashData_t *entry = *curPos;

    if (!strcmp(key, entry->key))
      return curPos;


    pos = hash1(key, table->size, pos + (attempt << 1));
    curPos = &table->entries[pos];
  } while (*curPos != NULL && attempt++ < LOOKUP_ATTEMPTS);

  if (attempt >= LOOKUP_ATTEMPTS)
    return NULL;

  return curPos;
}


int HashTable_copy(HashTable_t *dest, HashTable_t *src) {

  //make sure the dest table is at least as large as the source
  if (dest->size < src->size)
    return -1;

  //rehash all entries and copy them over
  size_t e = 0;
  for (e = 0; e < src->size; e++) {

    //skip null entries
    if (src->entries[e] == NULL)
      continue;

    //have a valid entry
    HashData_t **pos = HashTable_getEntry(dest, src->entries[e]->key);
    if (!pos) {
      return -1;
    }

    if (*pos == NULL)
      *pos = src->entries[e];
    else {
      return -1;
    }

  }

  return 0;
}

HashTable_t *HashTable_init(size_t size) {

  if (size <= 0)
    return NULL;

  HashTable_t *table = calloc(1, sizeof(HashTable_t));
  if (!table) {
    SYSLOG(LOG_ERR, "HashTable_init: Error allocating symbol table\n");
    return NULL;
  }
  table->size = size;
  table->entries = calloc(size, sizeof(HashData_t * ));
  if (!table->entries) {
    SYSLOG(LOG_ERR, "HashTable_init: Error allocating symtable entries\n");
    free(table);
    return NULL;
  }

  return table;
}

void HashTable_destroy(HashTable_t *table) {

  if (!table)
    return;


  if (table->entries) {

    size_t i = 0;
    for (i = 0; i < table->size; i++) {
      //skip empty indexes
      if (!table->entries[i])
        continue;

      HashData_t *curIndex = table->entries[i];
      HashData_destroy(curIndex);
    }

    free(table->entries);
  }

  memset(table, 0, sizeof(HashTable_t));
  free(table);
}


int HashTable_resize(HashTable_t *table, size_t size) {

  HashTable_t *newTable = NULL;
  int status = 0;
  do {
    newTable = HashTable_init(size);
    if (!newTable) {
      SYSLOG(LOG_ERR, "HashTable_resize: Error initializing new table for resize");
      return -1;
    }

    status = HashTable_copy(newTable, table);

    //error copying data, resize the table 
    if (status) {
      size = growSize(size);
      free(newTable);
    }
  } while (status);

  //then free old table, and assign new table
  free(table->entries);

  //make original table point to new table stuff
  table->entries = newTable->entries;
  table->size = newTable->size;
  free(newTable);
  //and done
  return 0;
}

void HashTable_print(FILE *output, HashTable_t *table) {

  if (!table || !table->entries)
    return;

  //loop through all symbol table entries
  size_t i = 0;
  for (i = 0; i < table->size; i++) {

    //skip empty entries
    if (!table->entries[i])
      continue;

    //skip entries where key may not be set (malformed entries)
    HashData_t *entry = table->entries[i];
    if (!entry->key)
      continue;

    HashData_print(output, entry);
  } //for each table element
}

HashData_t **HashTable_add(HashTable_t *table, HashData_t *data) {

  if (!table || !table->entries)
    return NULL;

  //if we somehow managed to fill the table, make it grow
  if (table->count >= table->size)
    HashTable_resize(table, growSize(table->size));


  HashData_t **position = HashTable_getEntry(table, data->key);

  if (!position) {
    HashTable_resize(table, growSize(table->size));
    //try adding the data again
    return HashTable_add(table, data);;

  }
  if (!*position) {
    (*position) = data;
    table->count++;
  } else {
    //SYSLOG(LOG_ERR, "HashTable_add: Error adding new entry");
    //entry already exists, overwrite
    HashData_destroy(*position);
    (*position) = data;
  }

  return position;
}

HashData_t *HashTable_find(HashTable_t *table, char *key) {

  HashData_t **position = HashTable_getEntry(table, key);

  if (!position || !*position) {
    return NULL;
  }

  return *position;
}


