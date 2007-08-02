/*****************************************************************************
 * \file hash.c
 * AUTHOR: David Crawshaw, Chris Park
 * $Revision: 1.1 $
 * \brief: Object for hashing.
 ****************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "hash.h"
#include "carp.h"
#include "parse_arguments.h"
#include "crux-utils.h"
#include "objects.h"
#include "parameter.h"



// Table is sized by primes to minimise clustering.
static const unsigned int sizes[] = {
    53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317,
    196613, 393241, 786433, 1572869, 3145739, 6291469, 12582917, 25165843,
    50331653, 100663319, 201326611, 402653189, 805306457, 1610612741
};

static const unsigned int sizes_count = sizeof(sizes) / sizeof(sizes[0]);
static const float load_factor = 0.65;

/**
 * \struct record
 * \brief record for each value/key pair
 */
struct record {
  unsigned int hash;
  char* key;
  void* value;
  int count;
};

/**
 * \struct hash
 * \brief hash table, contains the records
 */
struct hash {
  RECORD_T* records;
  unsigned int records_count;
  unsigned int size_index;
};

/**
 * add key and value to hash table.
 *\returns TRUE if successfully adds to new record, else FALSE
 */
BOOLEAN_T add_hash_when_grow(
  HASH_T* h, ///< Hash object to add -in/out
  char *key, ///< key of the record to add -in
  void *value, ///< value to add to be hashed if needed -in
  int count  ///< the count of the record to grow -in
  );

/**
 * Increase the size of hash table
 *\returns the TRUE if successfully inceased hash table size, else FALSE
 */
static int hash_grow(
  HASH_T* h ///< Hash object to grow -out
  )
{
  unsigned int i;
  RECORD_T* old_recs;
  unsigned int old_recs_length;
  
  old_recs_length = sizes[h->size_index];
  old_recs = h->records;
  
  if (h->size_index == sizes_count - 1){
    return FALSE;
  }
  //increase larger hash table
  if ((h->records = mycalloc(sizes[++h->size_index],
                             sizeof(RECORD_T))) == NULL) {
    h->records = old_recs;
    return FALSE;
  }
  
  h->records_count = 0;
  
  // rehash table
  for (i=0; i < old_recs_length; i++){
    if (old_recs[i].hash && old_recs[i].key){
      add_hash_when_grow(h, old_recs[i].key, old_recs[i].value, old_recs[i].count);
    }
  }
  
  free(old_recs);
  
  return TRUE;
}

/**
 * hash algorithm 
 * \returns the array slot
 */
static unsigned int strhash(
  char *str ///< string into the hash function -in
  )
{
  int c;
  int hash = 5381;
  while ((c = *str++)){
    hash = hash * 33 + c;
  }
  return hash == 0 ? 1 : hash;
}

/**
 * Create new hashtable with capacity.
 *\returns the hash table
 */
HASH_T* new_hash(
  unsigned int capacity ///< The capacity of the new hash table
  ) 
{
  HASH_T* h;
  unsigned int i, sind;
  
  capacity /= load_factor;
  
  for (i=0; i < sizes_count; i++) 
    if (sizes[i] > capacity) { sind = i; break; }
  
  if ((h = malloc(sizeof(HASH_T))) == NULL) return NULL;
  if ((h->records = mycalloc(sizes[sind], sizeof(RECORD_T))) == NULL) {
    free(h);
    return NULL;
  }
  
  h->records_count = 0;
  h->size_index = sind;
  
  return h;
}

/**
 * free hashtable
 */
void free_hash(
  HASH_T* h ///< Hash object to free -in
  )
{
  unsigned int idx = 0;
  unsigned int size = sizes[h->size_index];
  //free up all key strings
  for(; idx < size; ++idx){
    if(h->records[idx].key != NULL){
      free(h->records[idx].key);
    }
  }
  
  free(h->records);
  free(h);
}

/**
 * add key and value to hash table.
 * Must add a heap allocated key, value may be NULL
 *\returns TRUE if successfully adds to new record, else FALSE
 */
BOOLEAN_T add_hash(
  HASH_T* h, ///< Hash object to add -in/out
  char *key, ///< key of the record to add -in
  void *value ///< value to add to be hashed if needed -in
  )
{
    RECORD_T* recs;
    int rc;
    unsigned int off, ind, size, code;

    if (key == NULL || *key == '\0') return FALSE;
    if (h->records_count > sizes[h->size_index] * load_factor) {
        rc = hash_grow(h);
        if (rc) return FALSE;
    }

    code = strhash(key);
    recs = h->records;
    size = sizes[h->size_index];

    ind = code % size;
    off = 0;

    //probe down until reach open slot
    //Quadratic probing used
    while (recs[ind].key){     
      //if find duplicate key, thus identical item
      if ((code == recs[ind].hash) && recs[ind].key &&
          strcmp(key, recs[ind].key) == 0){
        //increment count
        ++recs[ind].count;
        free(key);
        return TRUE;
      }
      else{
        //continue to search
        ind = (code + (int)pow(++off,2)) % size;
      }
    }
    
    recs[ind].hash = code;
    recs[ind].key = key;
    recs[ind].value = value;
    recs[ind].count = 1;

    h->records_count++;
    
    return TRUE;
}

/**
 * add key and value to hash table.
 *\returns TRUE if successfully adds to new record, else FALSE
 */
BOOLEAN_T add_hash_when_grow(
  HASH_T* h, ///< Hash object to add -in/out
  char *key, ///< key of the record to add -in
  void *value, ///< value to add to be hashed if needed -in
  int count  ///< the count of the record to grow -in
  )
{
    RECORD_T* recs;
    int rc;
    unsigned int off, ind, size, code;

    if (key == NULL || *key == '\0') return FALSE;
    if (h->records_count > sizes[h->size_index] * load_factor) {
        rc = hash_grow(h);
        if (rc) return FALSE;
    }

    code = strhash(key);
    recs = h->records;
    size = sizes[h->size_index];

    ind = code % size;
    off = 0;

    //probe down until reach open slot
    while (recs[ind].key){
      ind = (code + (int)pow(++off,2)) % size;
    }
    
    recs[ind].hash = code;
    recs[ind].key = key;
    recs[ind].value = value;
    recs[ind].count = count;
    
    h->records_count++;
    
    return TRUE;
}

/**
 * Get the value of the record for the hash key
 *\return the value for the hash record, returns NULL if can't find key
 */
void* get_hash_value(
  HASH_T* h, ///< working hash object -in
  char *key  ///< the key of the record to retrieve -in
  )
{
  RECORD_T* recs;
  unsigned int off, ind, size;
  unsigned int code = strhash(key);
  
  recs = h->records;
  size = sizes[h->size_index];
  ind = code % size;
  off = 0;
  
  // search on hash which remains even if a record has been removed,
  // so remove_hash() does not need to move any collision records
  while (recs[ind].hash) {
    if ((code == recs[ind].hash) && recs[ind].key &&
        strcmp(key, recs[ind].key) == 0)
      return recs[ind].value;
    ind = (code + (int)pow(++off,2)) % size;
  }
  
  return NULL;
}

/**
 * Get the count of the record for the hash key
 *\return the count for the hash record, returns -1 if can't find key
 */
int get_hash_count(
  HASH_T* h, ///< working hash object -in
  char *key  ///< the key of the record to retrieve -in
  )
{
  RECORD_T* recs;
  unsigned int off, ind, size;
  unsigned int code = strhash(key);
  
  recs = h->records;
  size = sizes[h->size_index];
  ind = code % size;
  off = 0;
  
  // search on hash which remains even if a record has been removed,
  // so remove_hash() does not need to move any collision records
  while (recs[ind].hash) {
    if ((code == recs[ind].hash) && recs[ind].key &&
        strcmp(key, recs[ind].key) == 0)
      return recs[ind].count;
    ind = (code + (int)pow(++off,2)) % size;
  }
  
  return -1;
}

/**
 * Remove key from table, returning value.
 *\returns the value of removing record, returns NULL if can't find key
 */
void* remove_hash(
  HASH_T* h, ///< working hash object -in
  char *key  ///< the key of the record to remove -in
  )
{
  unsigned int code = strhash(key);
  RECORD_T* recs;
  void * value;
  unsigned int off, ind, size;
  
  recs = h->records;
  size = sizes[h->size_index];
  ind = code % size;
  off = 0;
  
  while (recs[ind].hash) {
    if ((code == recs[ind].hash) && recs[ind].key &&
        strcmp(key, recs[ind].key) == 0) {
      // do not erase hash, so probes for collisions succeed
      value = recs[ind].value;
      //free key
      free(recs[ind].key);
      recs[ind].key = 0;
      recs[ind].value = 0;
      h->records_count--;
      return value;
    }
    ind = (code + (int)pow(++off, 2)) % size;
  }
  
  return NULL;
}

/**
 *\returns total number of keys in the hashtable.
 */  
unsigned int hash_size(
  HASH_T* h ///< working hash object -in
  )
{
  return h->records_count;
}


/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * End:
 */
