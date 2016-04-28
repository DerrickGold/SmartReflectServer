#ifndef __SYMBOL_TABLE_H__
#define __SYMBOL_TABLE_H__

#include <stdbool.h>

typedef struct HashData_s {
    char *key, *value;
} HashData_t;


typedef struct HashTable_s {
    size_t count, size;
    HashData_t **entries;

} HashTable_t;


/*
 * HashData_create:
 *  Create a symbol for entry in the symbol table.
 *
 * Arguments:
 *  key*: string to use for lookup.
 *  data*: string or integer data to associate with key
 *
 *
 * Returns:
 *  Pointer to new populated symbol instance. NULL if symbol
 *  creation fails in any way.
 */
HashData_t *HashData_create(char *key, char *value);


/*
 * HashData_print:
 *  Print output a symbol table entry
 *
 * Arguments:
 *  output: File stream to write to
 *  entry: symbol to write output of.
 *
 */
void HashData_print(FILE *output, HashData_t *entry);

/*
 * HashTable_init
 *  Initialize a symbol table instance.
 *
 * Arguments:
 *  size: initial number of elements to allocate in symbol table.
 *
 * Returns:
 *  A pointer to a Symbol table instance. NULL if any error occured
 *  in initialization.
 */
HashTable_t *HashTable_init(size_t size);

/*
 * HashTable_destroy:
 *  Free all memory allocated by a symbol table instance.
 *  Does not free key pointers nor pointers used in symbol
 *  data.
 *
 * Arguments:
 *  table: Symbol table to destroy
 *
 */
void HashTable_destroy(HashTable_t *table);

/*
 * HashData_getEntry:
 *  Get a pointer to the symbols position in the symbol table.
 *
 * Arguments:
 *  table: Symbol table instance to search in
 *  key: Key to look up symbol with
 *  
 * Returns:
 *  A pointer to a location in the symbol table where the
 *  symbol should reside. Does not return the symbol itself.
 *  To get the symbol from this location, one just needs to
 *  dereference the return value, or use HashTable_find instead.
 */
HashData_t **HashTable_getEntry(HashTable_t *table, char *key);

/*
 * HashTable_add:
 *  Add symbol to symbol table.
 *
 * Arguments:
 *  table: Symbol table to add symbol into
 *  data: symbol to add into symbol table.
 *
 * Returns:
 *  Location in Symbol Table in which the symbol was added to.
 *  NULL if no table or symbol arguments given.
 */
HashData_t **HashTable_add(HashTable_t *table, HashData_t *data);


/*
 * HashTable_find:
 *  A wrapper for HashTable_getEntry. Retreive a symbol 
 *  from a symbol table.
 *
 * Arguments:
 *  table: Symbol table to search for symbol in
 *  key: Key to look up symbol with
 *
 * Returns:
 *  A pointer to a symbol instance.
 */
HashData_t *HashTable_find(HashTable_t *table, char *key);

/*
 * HashTable_print:
 *  Print out a symbol table.
 *
 * Arguments:
 *  output: file stream to write table output to
 *  talbe: the table to print out.
 */
void HashTable_print(FILE *output, HashTable_t *table);


#endif

