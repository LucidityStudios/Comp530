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
};

static struct list_node* list_head = NULL;

/* A static mutex */
static pthread_mutex_t mutex;
static int count = 0;
static pthread_cond_t cv_low, cv_high;

static volatile int done = 0;

/* Initialize the mutex. */
int init (int numthreads) {
    /* Your code here */
    //pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
    pthread_mutex_init(&mutex, NULL);
    /* Temporary code to suppress compiler warnings about unused variables.
     * You should remove the following lines once this file is implemented.
     */
    (void)list_head;
    //(void)mutex;
    (void)count;
    //(void)cv_low;
    pthread_cond_init(&cv_low, NULL);
    pthread_cond_init(&cv_high, NULL);
    //(void)cv_high;
    /* End temporary code */
    return 0;
}
/* Return 1 on success, 0 on failure.
 * Should set the reference count up by one if found; add if not.*/
int reference (int key) {
    /* Your code here */
    pthread_mutex_lock(&mutex);
    while(count >= HIGH_WATER_MARK) // used while loop, dont know who signals to use pthread_cond_signal() instead
        pthread_cond_wait(&cv_high, &mutex);

    int found = 0;
    struct list_node* cursor = list_head;
    struct list_node* last = NULL;

    while(cursor) {
        if (cursor->key < key) {
            last = cursor;
            cursor = cursor->next;
        } else {
            if (cursor->key == key) {
                cursor->refcount++;
                found++;
            }
            break;
        }

    }

    if (!found) {
        // Handle 2 cases: the list is empty/we are trying to put this at the front
        // and we want to insert somewhere in the middle or end of the list
        struct list_node* new_node = malloc(sizeof(struct list_node));
        if (!new_node){  // signal cv_low if malloc failed, then unlock. 
            pthread_cond_signal(&cv_low);
            pthread_mutex_unlock(&mutex);
            return 0;
        }
        count++;
        new_node ->key = key;
        new_node->refcount = 1;
        new_node->next = cursor;


        if (last == NULL)
            list_head = new_node;
        else
            last->next = new_node;
    }
    pthread_cond_signal(&cv_low); 
    pthread_mutex_unlock(&mutex);

    return 1;
}
/* Do a pass through all elements, either decrement the reference count,
 * or remove if it hasn't been referenced since last cleaning pass.
 *
 * check_water_mark: If 1, block until there are more elements in the cache
 * than the LOW_WATER_MARK.  This should only be 0 during self-testing or in
 * single-threaded mode.
 */
void clean(int check_water_mark) { // idk what to do about check watermark
    /* Your code here */
    pthread_mutex_lock(&mutex);
    if (check_water_mark == 1){
        while(count <= LOW_WATER_MARK)
            pthread_cond_wait(&cv_low, &mutex); //block this tread until there is references to clean
    }
    struct list_node* cursor = list_head;
    struct list_node* last = NULL;

    while(cursor) {
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
            free(cursor);
            cursor = tmp;
            count--;
        }
        else{
            last = cursor;
            cursor = cursor->next;
        }
    }
    pthread_cond_signal(&cv_high);
    pthread_mutex_unlock(&mutex);


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
