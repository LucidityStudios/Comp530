/* -*- mode:c; c-file-style:"k&r"; c-basic-offset: 4; tab-width:4; indent-tabs-mode:nil; mode:auto-fill; fill-column:78; -*- */
/* vim: set ts=4 sw=4 et tw=78 fo=cqt wm=0: */

/* @* Place your name here, and any other comments *@
 * @* that deanonymize your work inside this syntax *@
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "lru.h"

/* Define the simple, singly-linked list we are going to use for tracking lru */
struct list_node {
    struct list_node* next;
    int key;
    int refcount;
    // Protects this node's contents
    pthread_mutex_t mutex;
};

static struct list_node* list_head = NULL;

/* A static mutex; protects the count and head.
 * XXX: We will have to tolerate some lag in updating the count to avoid
 * deadlock. */
static pthread_mutex_t mutex;
static int count = 0;
static pthread_cond_t cv_low, cv_high;

static volatile int done = 0;

/* Initialize the mutex. */
int init (int numthreads) {
    /* Your code here */

    /* Temporary code to suppress compiler warnings about unused variables.
     * You should remove the following lines once this file is implemented.
     */
    pthread_mutex_init(&mutex, NULL); 
    //(void)list_head;
    //(void)count;
    pthread_cond_init(&cv_low, NULL);
    pthread_cond_init(&cv_high, NULL);
    /* End temporary code */
    return 0;
}

/* Return 1 on success, 0 on failure.
 * Should set the reference count up by one if found; add if not.*/
int reference (int key) {
    /* Your code here */
    pthread_mutex_lock(&mutex); // inital lock global lock
//    printf("%ld : Lock 01\n", pthread_self());
    while(count >= HIGH_WATER_MARK) 
        pthread_cond_wait(&cv_high, &mutex); // wait will unlock and acquire lock, and lock once finished
//    pthread_mutex_unlock(&mutex); // unlock global lock
    
    int found = 0;
    struct list_node* cursor = list_head;
    struct list_node* last = NULL;
    pthread_mutex_unlock(&mutex); // unlock global mutex (it was locked inside wait impl) after node pointers are adjusted

    while(cursor) {
        pthread_mutex_lock(&cursor->mutex); // lock current cursor mutex
        if (cursor->key < key) {
            last = cursor;
            cursor = cursor->next;
            if (last != NULL)
                pthread_mutex_unlock(&last->mutex); // unlock "last" mutex (which is old cursor mutex that was locked)
        } else {
            if (cursor->key == key) {
                cursor->refcount++;
                pthread_mutex_lock(&mutex); // lock mutex while found is being incremented to protect found
                found++;
                pthread_mutex_unlock(&mutex);
            }
            if (cursor != NULL)
                pthread_mutex_unlock(&cursor->mutex);
            break;
        }

    }

    if (!found) {
        // Handle 2 cases: the list is empty/we are trying to put this at the front
        // and we want to insert somewhere in the middle or end of the list
        struct list_node* new_node = malloc(sizeof(struct list_node));
        if (!new_node){  // signal cv_low if malloc failed, then unlock. 
            pthread_cond_signal(&cv_low);
           // pthread_mutex_unlock(&mutex);
            return 0;
        }
        pthread_mutex_lock(&mutex); // lock global lock before incrementing count and unlock right after
 //   printf("%ld : Lock 02\n", pthread_self());
        count++;
        pthread_mutex_unlock(&mutex); 
        // we are locking the lock at new_node while node/node elements are being updated.
        pthread_mutex_lock(&new_node->mutex);
  //  printf("%ld : Lock 03\n", pthread_self());
        new_node ->key = key;
        new_node->refcount = 1;
        new_node->next = cursor;
        pthread_mutex_unlock(&new_node->mutex); // unlock new_node lock after done

        pthread_mutex_lock(&mutex); // lock global mutex while list pointers are being adjusted. unlock right after
   // printf("%ld : Lock 04\n", pthread_self());
        if (last == NULL)
            list_head = new_node;
        else
            last->next = new_node;
        pthread_mutex_unlock(&mutex);
    }
    pthread_cond_signal(&cv_low); // signal threads waiting on cv_low once reference is added
   // pthread_mutex_unlock(&mutex);

    return 1;
}

/* Do a pass through all elements, either decrement the reference count,
 * or remove if it hasn't been referenced since last cleaning pass.
 *
 * check_water_mark: If 1, block until there are more elements in the cache
 * than the LOW_WATER_MARK.  This should only be 0 during self-testing or in
 * single-threaded mode.
 */
void clean(int check_water_mark) {
    /* Your code here */
    pthread_mutex_lock(&mutex);
    //printf("%ld : Lock 05\n", pthread_self());
    if (check_water_mark == 1){
        while(count <= LOW_WATER_MARK)
            pthread_cond_wait(&cv_low, &mutex); //block this tread until there is references to clean
    }
    struct list_node* cursor = list_head;
    struct list_node* last = NULL;
    pthread_mutex_unlock(&mutex);
    while(cursor) {
        pthread_mutex_lock(&cursor->mutex); // lock cursor mutex while node elements are being updated.
     //   printf("%ld : Lock 06\n", pthread_self());
        cursor->refcount--;
        if (cursor->refcount == 0) {

            struct list_node* tmp = cursor;
            if (last) {
                last->next = cursor -> next;
            }
            else {
                list_head = cursor->next;
            }
            tmp = cursor->next;
            if (cursor != NULL) {
                pthread_mutex_unlock(&cursor->mutex); // you can only unlock cursor mutex here, after all modifications to cursor
                free(cursor);
            }
            cursor = tmp;
            count--;
        }
        else{
            last = cursor;
            cursor = cursor->next;
            /* OR unlock last mutex here, 
             * if first "if" statement is not entered. 
             * Last is updated to cursor and cursor is updated to next.
             */ 
            if (last != NULL)
                pthread_mutex_unlock(&last->mutex); 
        } 
    } 
    pthread_cond_signal(&cv_high);
   // pthread_mutex_unlock(&mutex);

}


/* Optional shut-down routine to wake up blocked threads.
   May not be required. */
void shutdown_threads (void) {
    /* Your code here */
    return;
}

/* Print the contents of the list.  Mostly useful for debugging. */
void print (void) {
    /* Your code here */
    printf("=== Starting list print ===\n");
    printf("=== Total count is %d ===\n", count);
    struct list_node* cursor = list_head;
    while(cursor) {
        printf("Key %d, Ref Count %d\n", cursor->key, cursor->refcount);
        cursor = cursor->next;
    }
    printf("=== Ending list print ===\n");
}
