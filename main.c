#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <unistd.h>
#define MAX 1000000
#define TICKET_LOCK_INITIALIZER { PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER }

typedef struct ticket_lock_t {
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	unsigned long queue_head, queue_tail;
} ticket_lock_t;



void ticket_lock(ticket_lock_t *ticket)
{
	unsigned long queue_me;

	if(pthread_mutex_lock(&ticket->mutex)!=0)
		{                                        
		perror("pthread_mutex_lock() error");                                       
		exit(1);                                                                    
	}  
	queue_me = ticket->queue_tail++;
	while (queue_me != ticket->queue_head)
	{
		if(pthread_cond_wait(&ticket->cond, &ticket->mutex)!=0)
			{                                        
				perror("pthread_cond_wait() error");                                       
				exit(1);                                                                    
			}  
	}
	if(pthread_mutex_unlock(&ticket->mutex)!=0)
		{                                        
		perror("pthread_mutex_unlock() error");                                       
		exit(1);                                                                    
	}  
}

void ticket_unlock(ticket_lock_t *ticket)
{
	if(pthread_mutex_lock(&ticket->mutex)!=0)
	{                                        
		perror("pthread_mutex_lock() error");                                       
		exit(1);                                                                    
	}  
	ticket->queue_head++;
	if(pthread_cond_broadcast(&ticket->cond)!=0)
	{                                  
		perror("pthread_cond_broadcast() error");                                       
		exit(1);                                                                    
	}
	if(pthread_mutex_unlock(&ticket->mutex)!=0)
		{                                        
		perror("pthread_mutex_unlock() error");                                       
		exit(1);                                                                    
	}  

}

ticket_lock_t queueMutex = TICKET_LOCK_INITIALIZER;
ticket_lock_t queueFIFOMutex = TICKET_LOCK_INITIALIZER;

struct Clientlist
{
    int numb_client;
    struct Clientlist *next;
};

pthread_cond_t chairs_available; //dostępne miejsca w poczekalni
pthread_cond_t barber_free; // fryzjer obecnie wolny
pthread_cond_t wake_up_the_barber; //obudzenie spiacego fryzjera
pthread_cond_t end_of_haircut; //wysylany sygnal o koncu ostrzyzenia klienta

pthread_mutex_t end_of_client_haircut; 
pthread_mutex_t condition_of_the_barber; 
bool  is_seat_taken = false; 
bool is_the_barber_asleep; 
   
int time_client;     
int time_barbera;

int customer_on_the_armchair=-1; 

void randwait(int x) {
    //usleep((rand()%x) * (rand()%MAX) +MAX );
}

struct Clientlist *not_accepted_client = NULL;
struct Clientlist *waiting_clients = NULL;

void writeMessage(struct Clientlist* list, char* message)
{
	struct Clientlist *pom = list;
    printf("%s ", message);
    while(pom!=NULL)
    {
        printf("%d ",pom->numb_client);
        pom = pom->next;
    }
    printf("\n");
}

void Customers_not_admitted(int cl)
{
    struct Clientlist *temp = (struct Clientlist*)malloc(sizeof(struct Clientlist));
    temp->numb_client = cl;
    temp->next = not_accepted_client;
    not_accepted_client = temp;
	
	writeMessage(not_accepted_client,"Client didn't get into the barbershop");
}

void Clients_waiting(int cl)
{
    struct Clientlist *temp = (struct Clientlist*)malloc(sizeof(struct Clientlist));
    temp->numb_client = cl;
    temp->next = waiting_clients;
    waiting_clients = temp;

	writeMessage(waiting_clients, "The customer is waiting:");
}

void delete_client(int cl)
{
    struct Clientlist *temp = waiting_clients;
    struct Clientlist *pop = waiting_clients;
    while (temp!=NULL)
    {
        if(temp->numb_client==cl)
        {
            if(temp->numb_client==waiting_clients->numb_client)
            {
                waiting_clients=waiting_clients->next;
                free(temp);
            }
            else
            {
                pop->next=temp->next;
                free(temp);
            }
            break;
        }
        pop=temp;
        temp=temp->next;
    }
	writeMessage(waiting_clients,"The customer is waiting:");
}

int customers_did_not_enter=0;
int available_seats_waiting_room=10;  
int free_chairs=10; 
bool is_barber_awake = false;

bool debug=false; 
pthread_mutex_t armchair; 
pthread_mutex_t waiting_room; 

void clientCameToWaitingRoom(int numb_client)
{
	free_chairs--;
    printf("Res:%d WRomm: %d/%d [in: %d]  - a seat was taken in the waiting room\n",customers_did_not_enter, available_seats_waiting_room-free_chairs, available_seats_waiting_room, customer_on_the_armchair);
    if(debug==true) 
	{
	    Clients_waiting(numb_client); 
	}
}

void clientGoToBarberArmchair(int numb_client)
{
	ticket_lock(&queueMutex);
	if(pthread_mutex_lock(&armchair)!=0)
	{                                        
		perror("pthread_mutex_lock() error");                                       
		exit(1);                                                                    
	}   
    while (is_seat_taken)
    {
        if(pthread_cond_wait(&barber_free, &armchair)!=0)//sygnal o wolnym fryzjerze, który skończył ścinać
			{                                        
				perror("pthread_cond_wait() error");                                       
				exit(1);                                                                    
			}  
    }    

    is_seat_taken = true;
    customer_on_the_armchair=numb_client; 
    is_barber_awake = true;
    if(pthread_cond_signal(&wake_up_the_barber)!=0) //Wchodzi na krzesło to budzimy fryzjera
	{                                        
    	perror("pthread_cond_signal() error");                                   
    	exit(2);                                                                    
    } 
	ticket_unlock(&queueMutex);
    if(pthread_mutex_unlock(&armchair)!=0)
	{                                        
		perror("pthread_mutex_unlock() error");                                       
		exit(1);                                                                    
	}  
    ticket_lock(&queueFIFOMutex); 		// klient zajmuje miejsce na końcu kolejki
	if(pthread_mutex_lock(&waiting_room)!=0)
	{                                        
		perror("pthread_mutex_lock() error");                                       
		exit(1);                                                                    
	}  
	ticket_unlock(&queueFIFOMutex);// // moze to zwolnic klient z odpowiednim ticketem
    free_chairs++; 
    printf("Res:%d WRomm: %d/%d [in: %d]  -  The client went to the barber\n",customers_did_not_enter, available_seats_waiting_room-free_chairs, available_seats_waiting_room,customer_on_the_armchair);
    
    if(debug==true)
    {
        delete_client(numb_client);
    }

    if(pthread_mutex_unlock(&waiting_room)!=0)
	{                                        
		perror("pthread_mutex_unlock() error");                                       
		exit(1);                                                                    
	}  
}

void clientResignFromTheVisit(int numb_client)
{
    customers_did_not_enter++;
    printf("Res:%d WRomm: %d/%d [in: %d]  -  the client did not enter\n",customers_did_not_enter, available_seats_waiting_room-free_chairs, available_seats_waiting_room, customer_on_the_armchair);
    if(debug==true)
    {
        Customers_not_admitted(numb_client);
    }
	if(pthread_mutex_unlock(&waiting_room)!=0)
	{                                        
		perror("pthread_mutex_unlock() error");                                       
		exit(1);                                                                    
	}  
}

bool is_hair_cutted=false; 
bool end_work = false;

void waiting_for_the_end_of_the_cut(int numb_client)
{
        if(pthread_mutex_lock(&end_of_client_haircut))
		{                                        
			perror("pthread_mutex_lock() error");                                       
		 	exit(1);                                                                    
		}  
        while (is_hair_cutted==false)
        {
            pthread_cond_wait(&end_of_haircut, &end_of_client_haircut);// skonczyłem ścinać , ostani klient
        }
        is_hair_cutted=false; //  resetowanie aby przyszedł kolejny
        if(pthread_mutex_unlock(&end_of_client_haircut)!=0) //odblokowanie ostatniego klienta
		{                                        
			perror("pthread_mutex_unlock() error");                                       
			exit(1);                                                                    
		}  
}

void seat_unlocking(int numb_client)
{
        if(pthread_mutex_lock(&armchair)!=0)
		{                                        
			perror("pthread_mutex_unlock() error");                                       
			exit(1);                                                                    
	    }  
        is_seat_taken = false;
        if(pthread_cond_signal(&barber_free)!=0)
		{                                        
			perror("pthread_cond_signal() error");                                       
			exit(1);                                                                    
	    }  
        if(pthread_mutex_unlock(&armchair)!=0)
		{                                        
			perror("pthread_mutex_unlock() error");                                       
		 exit(1);                                                                    
		}  

}

void *Client(void *numb_client)
{
    randwait(8);
    if(pthread_mutex_lock(&waiting_room)!=0)
	{                                        
		perror("pthread_mutex_lock() error");                                       
		exit(1);                                                                    
	}    
    if(free_chairs>0)
    {
		clientCameToWaitingRoom(*(int *)numb_client);
		if(pthread_mutex_unlock(&waiting_room)!=0)
		{                                        
		perror("pthread_mutex_unlock() error");                                       
		exit(1);                                                                    
	    }  
		clientGoToBarberArmchair(*(int *)numb_client);
		printf("Res:%d WRomm: %d/%d [in: %d]  -  The barber is cutting the client's hair\n",customers_did_not_enter, available_seats_waiting_room-free_chairs, available_seats_waiting_room, customer_on_the_armchair);

		waiting_for_the_end_of_the_cut(*(int *)numb_client);
		seat_unlocking(*(int *)numb_client);
    }
    else
    {
		clientResignFromTheVisit(*(int *)numb_client);
    }
}



void wait_for_the_barber_to_wake_up()
{
    while(is_barber_awake == false)
    {
        if(pthread_cond_wait(&wake_up_the_barber, &condition_of_the_barber)!=0)
		{
		    perror("pthread_cond_timedwait() error");                                   
    		exit(7);                                                                    
 		} 
    }

    is_barber_awake = false;
    pthread_mutex_unlock(&condition_of_the_barber);
}

void clientHaircut()
{
	randwait(2);
    if(pthread_mutex_lock(&end_of_client_haircut)!=0)
	{                                        
		perror("pthread_mutex_lock() error");                                       
		exit(1);                                                                    
	}  
    is_hair_cutted = true;
    if(pthread_cond_signal(&end_of_haircut)!=0) //klient ścięty
	{                                        
    	perror("pthread_cond_signal() error");                                   
    	exit(2);                                                                    
    } 
    if(pthread_mutex_unlock(&end_of_client_haircut)!=0)
	{                                        
		perror("pthread_mutex_unlock() error");                                       
		exit(1);                                                                    
	}  
}

void *Barber()
{
	if(pthread_mutex_lock(&condition_of_the_barber)!=0)
	{                                        
		perror("pthread_mutex_lock() error");                                       
		exit(1);                                                                    
	}  
    while(!end_work)
    {

        wait_for_the_barber_to_wake_up();
        if(end_work)
        {
            printf("The barber is going home for the day.\n");
            break;
        }
    	clientHaircut(); 
    }

}

void init_conditional_variables()
{
	if(pthread_cond_init(&chairs_available, NULL)!=0)
	{
	    perror("pthread_cond_init() error");                                        
    	exit(1);                                                                    
    }  
    if(pthread_cond_init(&barber_free, NULL)!=0)
	{
	    perror("pthread_cond_init() error");                                        
    	exit(1);                                                                    
    }  
    if(pthread_cond_init(&wake_up_the_barber, NULL)!=0)
	{
	    perror("pthread_cond_init() error");                                        
    	exit(1);                                                                    
    }  
    if(pthread_cond_init(&end_of_haircut, NULL)!=0)
	{
	    perror("pthread_cond_init() error");                                        
    	exit(1);                                                                    
    }  
}

void init_mutex()
{
    if(pthread_mutex_init(&armchair,NULL)==-1)
	{
		perror("mutex_init error");                                                 
        exit(2); 
	}
    if(pthread_mutex_init(&waiting_room,NULL)==-1)
	{
		perror("mutex_init error");                                                 
        exit(2); 
	}
    if(pthread_mutex_init(&end_of_client_haircut,NULL)==-1)
	{
		perror("mutex_init error");                                                 
        exit(2); 
	}
    if(pthread_mutex_init(&condition_of_the_barber,NULL)==-1)
	{
		perror("mutex_init error");                                                 
        exit(2); 
	}
}

void destroyMutex()
{
    pthread_mutex_destroy(&armchair);
    pthread_mutex_destroy(&waiting_room);
    pthread_mutex_destroy(&end_of_client_haircut);
    pthread_mutex_destroy(&condition_of_the_barber);
}

void destory_conditional_variables()
{
	if(pthread_cond_destroy(&chairs_available)!=0)
	{
	    perror("pthread_cond_destroy() error");                                     
        exit(3);                                                                    
    } 
    if(pthread_cond_destroy(&barber_free)!=0)
	{
	    perror("pthread_cond_destroy() error");                                     
        exit(3);                                                                    
    } 
    if(pthread_cond_destroy(&wake_up_the_barber)!=0)
	{
	    perror("pthread_cond_destroy() error");                                     
        exit(3);                                                                    
    } 
    if(pthread_cond_destroy(&end_of_haircut)!=0)
	{
	    perror("pthread_cond_destroy() error");                                     
        exit(3);                                                                    
    } 
}

void deleteList(struct Clientlist* list)
{
	while (list != NULL)
	{
		struct Clientlist* tmp = list;
		list = list->next;
		free(tmp);
	}

}

int main(int argc, char *argv[])
{
	pthread_t barber_thread;
	pthread_t *clients_threads;
	int *t_clients;
	int number_of_clients=25;
    int choice = 0;
    srand(time(NULL));

    static struct option param[] =
    {
        {"Client", required_argument, NULL, 'c'},
        {"Armchair", required_argument, NULL, 'a'},
        {"debug", no_argument, NULL, 'd'}
    };
	
    while((choice = getopt_long(argc, argv, "c:a:d",param,NULL)) != -1)
    {
        switch(choice)
        {
			case 'c': 
				number_of_clients=atoi(optarg);
				break;
			case 'a': 
				free_chairs=atoi(optarg);
				available_seats_waiting_room=atoi(optarg);
				break;
			case 'd':
				debug=true;
				break;
			default: /* '?' */
            	fprintf(stderr, "Usage: %s [-option] [val]\n",argv[0]);
            	exit(EXIT_FAILURE);
        }
    }

    clients_threads = malloc(sizeof(pthread_t)*number_of_clients);
    t_clients=malloc(sizeof(int)*number_of_clients);

    int i;
    for (i=0; i<number_of_clients; i++)
    {
        t_clients[i] = i;
    }

	init_conditional_variables();
	init_mutex();

    if(pthread_create(&barber_thread, NULL, Barber, NULL)!=0)
	{
		perror("create Thread");
        pthread_exit(NULL);
	}
    for (i=0; i<number_of_clients; ++i)
    {
        if(pthread_create(&clients_threads[i], NULL, Client, (void *)&t_clients[i])!=0)
		{
			 perror("create Thread");
             pthread_exit(NULL);
		}
    }

    for (i=0; i<number_of_clients; i++)
    {
        if(pthread_join(clients_threads[i],NULL)!=0)
		{
		perror("error joining thread:");
		}
    }
    
	end_work=true;
    is_barber_awake = true;
	if(pthread_cond_signal(&wake_up_the_barber)!=0)
	{                                        
    	perror("pthread_cond_signal() error");                                   
    	exit(2);                                                                    
    } 
	if(pthread_join(barber_thread,NULL)!=0)
	{
		perror("error joining thread:");
	}

    destory_conditional_variables();
	destroyMutex();
	
    deleteList(waiting_clients);
    deleteList(not_accepted_client);

    return 0;
}
