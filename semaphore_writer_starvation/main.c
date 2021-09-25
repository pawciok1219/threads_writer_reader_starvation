#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "errno.h"



int number_of_writers;
int number_of_readers;

int active_number_of_writers;
int active_number_of_readers;

pthread_t *writer_threads;
pthread_t *reader_threads;

sem_t reader_sem;
sem_t writer_sem;

int errnum;

void *writer(void *arg) {
    int *index = (int*) arg;
    while(1) {

        if (sem_wait(&writer_sem) == -1) {
            if (errno == EINTR) continue;
            else {
                errnum = errno;
                fprintf(stderr, "Error in sem_wait writer_Sem at Writer function, %s", strerror(errnum));
                exit(EXIT_FAILURE);
            }
        }
        active_number_of_writers++;

        printf("ReaderQ: %d WriterQ: %d [IN: R:%d W:%d], Wchodzi Pisarz id: %d" "\n",
               number_of_readers - active_number_of_readers,
               number_of_writers - active_number_of_writers,
               active_number_of_readers,active_number_of_writers,*index);

        usleep(250*1000);

        active_number_of_writers--;

        printf("ReaderQ: %d WriterQ: %d [IN: R:%d W:%d], Wychodzi Pisarz id: %d" "\n",
               number_of_readers - active_number_of_readers,
               number_of_writers - active_number_of_writers,
               active_number_of_readers,active_number_of_writers,*index);


        if (sem_post(&writer_sem) == -1) {
            perror("sem_post() writer sem in writer function failed \n");
            exit(EXIT_FAILURE);
        }
    }

    return NULL;

}

void *reader(void *arg) {
    int *index = (int*) arg;
    while (1) {
        if(sem_wait(&reader_sem) == -1) {
            if (errno == EINTR) continue;
            else {
                perror("Error with sem wait reader sem in reader function!");
                exit(EXIT_FAILURE);
            }
        }// wejscie do sekcji krytycznej

        if (active_number_of_readers + 1 == 1) { // jezeli czytelnik jest jedyna osoba w bibliotece
            if (sem_wait(&writer_sem) == -1) { // pisarze musza czekac w kolejce
                if (errno == EINTR) continue;
                else {
                    errnum = errno;
                    fprintf(stderr, "Error in sem_wait reader function, %s", strerror(errnum));
                    exit(EXIT_FAILURE);
                }
            }
        }

        active_number_of_readers++; // do biblioteki bez kolejki wchodzi czytelnik

        printf("ReaderQ: %d WriterQ: %d [in: R:%d W:%d], Wchodzi Czytelnik id: %d""\n",
               number_of_readers - active_number_of_readers,
               number_of_writers - active_number_of_writers,
               active_number_of_readers,
               active_number_of_writers,
               *index
        );

        if(sem_post(&reader_sem) == -1) {
            perror("sem_post() reader_sem in reader function failed \n");
            exit(EXIT_FAILURE);
        }  // wyjscie z sekcji krytycznej

        // poczatek kodu wykonywanego przez czytelnika
        usleep(250 * 1000);

        // koniec kodu wykonywanego przez czytelnika

        if(sem_wait(&reader_sem) == -1) {
            if(errno == EINTR) continue;
            else{
                errnum = errno;
                fprintf(stderr,"Error in second sem_wait reader function, %s", strerror(errnum));
                exit(EXIT_FAILURE);
            }
        }// wejscie do sekcji krytycznej

        active_number_of_readers--; // czytelnik wychodzi z biblioteki

        printf("ReaderQ: %d WriterQ: %d [in: R:%d W:%d], Wychodzi Czytelnik id: %d""\n",
               number_of_readers - active_number_of_readers,
               number_of_writers - active_number_of_writers,
               active_number_of_readers,
               active_number_of_writers,
               *index
        );

        if (active_number_of_readers == 0) { // jezeli w bibliotece nie bedzie czytelnikow
            if (sem_post(&writer_sem) == -1) {
                perror("sem_post() writer_sem in reader function failed \n");
                exit(EXIT_FAILURE);
            } // informujemy pisarzy, ze biblioteka jest pusta
        }

        if (sem_post(&reader_sem) == -1) {
            perror("sem_post() reader_sem in reader function failed \n");
            exit(EXIT_FAILURE);
        }; // wyjscie z sekcji krytycznej

    }
}

int main(int argc, char* argv[]) {

    number_of_readers = 0;
    number_of_writers = 0;

    if( argc != 5 ){
        fprintf(stderr,"%s","Bład w przekazywaniu argumentów ilosci pisarzy i czytelnikow\n");
        exit(EXIT_FAILURE);
    }

    // MENU
    // - R wartosc (liczba czytelnikow ile ich mamy do dyspozycji)
    // - W wartosc (liczba pisarzy ilu mamy do dyspozycji)

    int choice;

    while ((choice = getopt(argc,argv,"R:W:")) != -1){

        switch (choice) {
            case 'R':
                number_of_readers = atoi(optarg);
                if( number_of_readers < 1) {
                    fprintf(stderr,"%s","Czytelnik musi byc przynajmniej jeden !");
                    exit(EXIT_FAILURE);
                }
                break;

            case 'W':
                number_of_writers = atoi(optarg);
                if( number_of_writers < 1) {
                    fprintf(stderr,"%s","Pisarz musi byc przynajmniej jeden !");
                    exit(EXIT_FAILURE);
                }
                break;

            default:
                fprintf(stderr,"%s","Podano nieprawidlowe argumenty");
                exit(EXIT_FAILURE);
        }
    }
    int rval = 0;

    active_number_of_readers = 0;
    active_number_of_writers = 0;

    rval = sem_init(&reader_sem,0,1); // Semafory zaczynaja prace na wartosci 1.
    if(rval) {
        perror("Error with sem init for reader_sem");
        exit(EXIT_FAILURE);
    }

    rval = sem_init(&writer_sem,0,1);
    if(rval) {
        perror("Error with sem init for writer_sem");
        exit(EXIT_FAILURE);
    }
    writer_threads = malloc(sizeof(pthread_t) * number_of_writers);
    reader_threads = malloc(sizeof(pthread_t) * number_of_readers);
    for (int i = 0; i < number_of_writers; ++i) {
        int *index = malloc(sizeof(int));
        *index = i;
        rval = pthread_create(&writer_threads[i],NULL,writer,index);
        if(rval){
            fprintf(stderr,"Error - pthread_create() return: %d\n",rval);
            exit(EXIT_FAILURE);
        }

    }
    for (int i = 0; i < number_of_readers; ++i) {
        int *index = malloc(sizeof(int));
        *index = i;
        rval = pthread_create(&reader_threads[i],NULL,reader,index);
        if(rval) {
            fprintf(stderr,"Error - pthread_create() return: %d\n",rval);
            exit(EXIT_FAILURE);
        }
    }


    for (int i = 0; i < number_of_writers; ++i) {
        void *index = NULL;

        rval = pthread_join(writer_threads[i],&index);
        if(rval)
        {
            fprintf(stderr,"Error - pthread_join() return: %d\n",rval);
            exit(EXIT_FAILURE);
        }
        free(index);
        index = NULL;
    }
    for (int i = 0; i < number_of_readers; ++i) {
        void *index = NULL;
        rval = pthread_join(reader_threads[i],&index);
        if(rval) {
            fprintf(stderr,"Error - pthread_join() return: %d\n",rval);
            exit(EXIT_FAILURE);
        }
        free(index);
        index = NULL;
    }

    free(reader_threads);
    free(writer_threads);

    pthread_exit(0);
}