#include "buffer.h"
#include <pthread.h>


// Creates a buffer with the given capacity
state_t* buffer_create(int capacity)
{
    state_t* buffer = (state_t*) malloc(sizeof(state_t));
    buffer->fifoQ = (fifo_t *) malloc ( sizeof (fifo_t));
    fifo_init(buffer->fifoQ,capacity);
    buffer->isopen = true;
    return buffer;
}


// Writes data to the given buffer
// This is a blocking call i.e., the function only returns on a successful completion of send
// In case the buffer is full, the function waits till the buffer has space to write the new data
// Returns BUFFER_SUCCESS for successfully writing data to the buffer,
// CLOSED_ERROR if the buffer is closed, and
// BUFFER_ERROR on encountering any other generic error of any sort
enum buffer_status buffer_send(state_t *buffer, void* data)
{
    int msg_size = get_msg_size(data);
    //Lock for close
    pthread_mutex_lock(&buffer->chclose);
    
    if(!buffer->isopen)
    {
        //unlock close
        pthread_mutex_unlock(&buffer->chclose);
        return CLOSED_ERROR;
    }
    pthread_mutex_unlock(&buffer->chclose);

    //lock
    
    pthread_mutex_lock(&(buffer->chmutex));
    //While the buffer is full,
    while (!(fifo_avail_size(buffer->fifoQ) > msg_size) && buffer->isopen) {

        
        pthread_cond_wait(&(buffer->chconsend), &(buffer->chmutex)); 

                                                                //wait untill it ok to send (more room).
                                                                //but breaks out the loop only.
                                                                //when there is enough to room to send.

    }

    if(!buffer->isopen)
    {
        //unlock close
        pthread_mutex_unlock(&(buffer->chmutex));
        return CLOSED_ERROR;
    }

    //add
    if(fifo_avail_size(buffer->fifoQ) > msg_size)
    {
	    buffer_add_Q(buffer,data);

        //signal no longer empty
        pthread_cond_signal(&(buffer->chconrec)); //now it is ok to try receive
        //unlock close
        pthread_mutex_unlock(&(buffer->chmutex));
    	return BUFFER_SUCCESS;	
    }
    else {
        //unlock close
        pthread_mutex_unlock(&(buffer->chmutex));
        return BUFFER_ERROR;
    }
}

// test_send_correctness 1
// Reads data from the given buffer and stores it in the function’s input parameter, data (Note that it is a double pointer).
// This is a blocking call i.e., the function only returns on a successful completion of receive
// In case the buffer is empty, the function waits till the buffer has some data to read
// Return BUFFER_SPECIAL_MESSSAGE for successful retrieval of special data "splmsg"
// Returns BUFFER_SUCCESS for successful retrieval of any data other than "splmsg"
// CLOSED_ERROR if the buffer is closed, and
// BUFFER_ERROR on encountering any other generic error of any sort

enum buffer_status buffer_receive(state_t* buffer, void** data)
{
    //lock close
    pthread_mutex_lock(&(buffer->chclose));
    //printf("in the receive chclose lock\n"); //fix
    if(!buffer->isopen)
    {
        //unlock close
        pthread_mutex_unlock(&(buffer->chclose));
        return CLOSED_ERROR;
    }
    pthread_mutex_unlock(&(buffer->chclose));

    //lock
    pthread_mutex_lock(&(buffer->chmutex));
    //printf("in the receive chmutex lock\n"); //fix

    while (!(buffer->fifoQ->avilSize < buffer->fifoQ->size)){
        
        pthread_cond_wait(&(buffer->chconrec), &(buffer->chmutex));//wait untill it ok to receive (more room added).
                                                                //but breaks out the loop only.
                                                                //when there is enough to room to send.

    }
    //printf("out of the while loop\n"); //fix

    if(!buffer->isopen)
    {
        //unlock close
        pthread_mutex_lock(&(buffer->chmutex));
        return CLOSED_ERROR;
    }

    if(buffer->fifoQ->avilSize < buffer->fifoQ->size)  // checking if there is something in the Q to remove
    {
        //printf("about to remove data in buffer\n"); //fix
    	buffer_remove_Q(buffer,data);
        pthread_cond_signal(&(buffer->chconsend)); //now it is ok to try receive

    	if(strcmp(*(char**)(data),"splmsg") ==0)
    	{
            //unlock close
            pthread_mutex_unlock(&(buffer->chmutex));
        	return BUFFER_SPECIAL_MESSSAGE;
    	}
        //unlock close
        pthread_mutex_unlock(&(buffer->chmutex));
    	return BUFFER_SUCCESS;
    }
    else {
        //unlock close
        pthread_mutex_unlock(&(buffer->chmutex));
        return BUFFER_ERROR;
    }
    
}


// Closes the buffer and informs all the blocking send/receive/select calls to return with CLOSED_ERROR
// Once the buffer is closed, send/receive/select operations will cease to function and just return CLOSED_ERROR
// Returns BUFFER_SUCCESS if close is successful,
// CLOSED_ERROR if the buffer is already closed, and
// BUFFER_ERROR in any other error case
enum buffer_status buffer_close(state_t* buffer)
{
    //lock close
    pthread_mutex_lock(&(buffer->chclose));
    pthread_mutex_lock(&(buffer->chmutex));
    if(!buffer->isopen)
    {
        //unlock close
        pthread_mutex_unlock(&(buffer->chmutex));
        pthread_mutex_unlock(&(buffer->chclose));
        return CLOSED_ERROR;
    }
    buffer->isopen = false;
    //unlock close
    pthread_mutex_unlock(&(buffer->chmutex));
    pthread_mutex_unlock(&(buffer->chclose));
    return BUFFER_SUCCESS;
    
}

// Frees all the memory allocated to the buffer , using own version of sem flags
// The caller is responsible for calling buffer_close and waiting for all threads to finish their tasks before calling buffer_destroy
// Returns BUFFER_SUCCESS if destroy is successful,
// DESTROY_ERROR if buffer_destroy is called on an open buffer, and
// BUFFER_ERROR in any other error case

enum buffer_status buffer_destroy(state_t* buffer)
{

    if(buffer->isopen)
    {
        return DESTROY_ERROR;
    }
    fifo_free(buffer->fifoQ);
    free(buffer);
    return BUFFER_SUCCESS;
}
