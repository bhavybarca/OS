#include "common.h"

//BHAVY SINGH 170212

/*Function templates. TODO*/

static int atomic_add(long *ptr, long val)
{
  int ret = 0;
  asm volatile(
      "lock add %%rsi, (%%rdi);"
      "pushf;"
      "pop %%rax;"
      "movl %%eax, %0;"
      : "=r"(ret)
      :
      : "memory", "rax");

  if (ret & 0x80)
    ret = -1;
  else if (ret & 0x40)
    ret = 0;
  else
    ret = 1;
  return ret;
}

void done_one(struct input_manager *in, int tnum)
{
  pthread_mutex_lock(&in->lock);
  in->being_processed[tnum - 1] = NULL;
  pthread_cond_broadcast(&in->cond);
  pthread_mutex_unlock(&in->lock);
}

int read_op(struct input_manager *in, op_t *op, int tnum)
{
  unsigned size = sizeof(op_t);
  pthread_mutex_lock(&in->lock);
  memcpy(op, in->curr, size - sizeof(unsigned long)); 
  if (op->op_type == GET || op->op_type == DEL)
  {
    in->curr += size - sizeof(op->datalen) - sizeof(op->data);
  }
  else if (op->op_type == PUT)
  {
    in->curr += size - sizeof(op->data);
    op->data = in->curr;
    in->curr += op->datalen;
  }
  else
  {
    pthread_mutex_unlock(&in->lock);
    return -1;
    // assert(0);
  }
  if (in->curr > in->data + in->size)
  {
    pthread_mutex_unlock(&in->lock);
    return -1;
  }
  in->being_processed[tnum - 1] = op;
  for (int i = 0; i < THREADS; i++)
  {
    if (i != tnum - 1 && in->being_processed[i] && in->being_processed[i]->key == op->key &&
        in->being_processed[i]->id < op->id &&
        (in->being_processed[i]->op_type != GET || (in->being_processed[i]->op_type == GET && op->op_type != GET)))
    {
      i = -1;
      pthread_cond_wait(&in->cond, &in->lock);
    }
  }
  pthread_mutex_unlock(&in->lock);
  return 0;
}

int lookup(hash_t *h, op_t *op)
{
  unsigned ctr;
  unsigned hashval = hashfunc(op->key, h->table_size);
  pthread_mutex_lock(&(h->table + hashval)->lock);
  hash_entry_t *entry = h->table + hashval;
  ctr = hashval;
  while ((entry->key || entry->id == (unsigned)-1) &&
         entry->key != op->key && ctr != hashval - 1)
  {
    pthread_mutex_unlock(&entry->lock);
    ctr = (ctr + 1) % h->table_size;
    pthread_mutex_lock(&(h->table + ctr)->lock);
    entry = h->table + ctr;
  }
  if (entry->key == op->key)
  {
    op->datalen = entry->datalen;
    op->data = entry->data;
    pthread_mutex_unlock(&entry->lock);
    return 0;
  }
  pthread_mutex_unlock(&entry->lock);
  return -1;
}

int insert_update(hash_t *h, op_t *op)
{
  unsigned ctr;
  unsigned hashval = hashfunc(op->key, h->table_size);
  pthread_mutex_lock(&(h->table + hashval)->lock);
  hash_entry_t *entry = h->table + hashval;

  assert(h && h->used < h->table_size);
  assert(op && op->key);

  ctr = hashval;

  while ((entry->key || entry->id == (unsigned)-1) &&
         entry->key != op->key && ctr != hashval - 1)
  {
    pthread_mutex_unlock(&entry->lock);
    ctr = (ctr + 1) % h->table_size;
    pthread_mutex_lock(&(h->table + ctr)->lock);
    entry = h->table + ctr;
  }

  if (entry->key == op->key)
  { //It is an update
    entry->id = op->id;
    entry->datalen = op->datalen;
    entry->data = op->data;
    pthread_mutex_unlock(&entry->lock);
    return 0;
  }
  else if (!entry->key && entry->id != (unsigned)(-1))
  { // Fresh insertion

    entry->id = op->id;
    entry->datalen = op->datalen;
    entry->key = op->key;
    entry->data = op->data;
    pthread_mutex_unlock(&entry->lock);
    atomic_add(&h->used, 1);
    return 0;
  }
  else
  {
    ctr = hashval;
    entry = h->table + hashval;
    pthread_mutex_unlock(&entry->lock);
    while (entry->key && ctr != hashval - 1)
    {
      pthread_mutex_unlock(&entry->lock);
      ctr = (ctr + 1) % h->table_size;
      pthread_mutex_lock(&(h->table + ctr)->lock);
      entry = h->table + ctr;
    }

    if (entry->key == op->key)
    { //It is an update
      entry->id = op->id;
      entry->datalen = op->datalen;
      entry->data = op->data;
      pthread_mutex_unlock(&entry->lock);
      return 0;
    }

    entry->id = op->id;
    entry->datalen = op->datalen;
    entry->key = op->key;
    entry->data = op->data;
    pthread_mutex_unlock(&entry->lock);
    atomic_add(&h->used, 1);
    return 0;
  }
  pthread_mutex_unlock(&entry->lock);
  return -1;
}

int purge_key(hash_t *h, op_t *op)
{
  unsigned ctr;
  unsigned hashval = hashfunc(op->key, h->table_size);
  pthread_mutex_lock(&(h->table + hashval)->lock);
  hash_entry_t *entry = h->table + hashval;

  ctr = hashval;
  while ((entry->key || entry->id == (unsigned)-1) &&
         entry->key != op->key && ctr != hashval - 1)
  {
    pthread_mutex_unlock(&entry->lock);
    ctr = (ctr + 1) % h->table_size;
    pthread_mutex_lock(&(h->table + ctr)->lock);
    entry = h->table + ctr;
  }

  if (entry->key == op->key)
  {
    entry->id = (unsigned)-1;
    entry->key = 0;
    entry->datalen = 0;
    entry->data = NULL;
    pthread_mutex_unlock(&entry->lock);
    atomic_add(&h->used, -1);
    return 0;
  }
  pthread_mutex_unlock(&entry->lock);
  return -1; // Bogus purge
}
