  #include <unistd.h>
  #include <stdio.h>
  #include <pthread.h>
  #include <string.h>
  #include <stdalign.h>
  #include <stddef.h>   // defines max_align_t


  union header {

    struct {
      size_t size;
      unsigned is_free;
      union header *next;
    } s;
   
    max_align_t _align;
  };

  typedef union header header_t;
  header_t *head, *tail;
  pthread_mutex_t global_malloc_lock = PTHREAD_MUTEX_INITIALIZER;

  header_t *get_free_block(size_t size){
    header_t *curr = head;
    while(curr){
      if(curr->s.is_free && curr->s.size >= size)
        return curr;
      curr = curr->s.next;
    }
    return NULL;
  }

  void *malloc(size_t size) {
    size_t total_size;
    void *block;
    header_t *header;
    if(!size)
      return NULL;

    pthread_mutex_lock(&global_malloc_lock);
    header = get_free_block(size);

    if(header){
      header->s.is_free = 0;
      pthread_mutex_unlock(&global_malloc_lock);
      return (void*)(header + 1);
    }

    total_size = sizeof(header_t) + size;
    block = sbrk(total_size);
    if (block == (void*) -1){
      pthread_mutex_unlock(&global_malloc_lock);
      return NULL;
    }
    
    //header is NULL at this point
    header = block;
    header->s.size = size;
    header->s.is_free = 0;
    header->s.next = NULL;

    if (!head)
      head = header;
    if(tail)
      tail->s.next = header;

    tail = header;
    pthread_mutex_unlock(&global_malloc_lock);
    return (void*)(header + 1);

  }

  void free(void *block){
    header_t *header, *tmp;
    void *programbreak;

    if(!block)
      return;

    pthread_mutex_lock(&global_malloc_lock);
    //check the address of the current block
    header = (header_t*)block - 1;
    programbreak = sbrk(0);
    //if the end of the block is at the program break 
    if((char*)block + header->s.size == programbreak){
      if (head == tail){
        head = tail = NULL;
      }else{
        tmp = head;
        //keep iterating until you reach the last node before tail
        //free the node and set curr node to tal
        while(tmp) {
          if(tmp->s.next == tail){
              tmp->s.next = NULL;
              tail = tmp;
            }
            tmp = tmp->s.next;
        }
      }
      sbrk(0 - sizeof(header_t) - header->s.size);
      pthread_mutex_unlock(&global_malloc_lock);
      return;
    }

    header->s.is_free = 1;
    pthread_mutex_unlock(&global_malloc_lock);
  }

  void *calloc(size_t num, size_t nsize){
    size_t size;
    void *block;
    if (!num || !nsize)
      return NULL;
    size = num * nsize;
    //check null overflow
    if(nsize != size/num)
      return NULL;

    block = malloc(size);
    if(!block)
      return NULL;
    
    memset(block, 0, size);
    return block;    
  }

  void *realloc(void *block, size_t size){
    header_t *header;
    void *ret;

    if (!block) {
      return size ? malloc(size): NULL;
    }

    if (size == 0){
      free(block);
      return NULL;
    }

    header = (header_t*)block - 1;
    if (header->s.size >= size)
      return block;
    ret = malloc(size);
    if (ret) {
      memcpy(ret, block, header->s.size);
      free(block);
    }
      return ret;
  }


 


