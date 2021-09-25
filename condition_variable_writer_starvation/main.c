#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

int number_of_writers;
int number_of_readers;

pthread_mutex_t mutex;
pthread_cond_t cond;

int waiting_writers_count;
int active_number_of_readers;
int active_number_of_writers;
int rval;
void Join_Pthread(int number, const pthread_t *pthread);

void *writer_function(void *arg) {
    int *id = (int*) arg;
    while (1) {
        rval = pthread_mutex_lock(&mutex); // wejscie do sekcji krytycznej
        if(rval) {
            fprintf(stderr,"Error with pthread mutex unlock return: %d",rval);
            exit(EXIT_FAILURE);
        }
        waiting_writers_count++; // bardzo wazna linia -> informacja, ze do kolejki wszedl pisarz, czytelnicy ustapia mu miejsca

        // pisarz bedzie czekal w kolejce, jezeli:
        // * istnieje inny pisarz, ktory pisze <- dwie osoby nie moga modyfikowac zasobow
        // * istnieje czytelnik, ktory korzysta z biblioteki <- nie mozna modyfikowac zasobow i jednoczesnie z nich korzystac
        while (active_number_of_writers > 0 || active_number_of_readers > 0) {
            rval = pthread_cond_wait(&cond, &mutex);
            if(rval) {
                fprintf(stderr,"Error with pthread cond wait return: %d",rval);
                exit(EXIT_FAILURE);
            }
        }

        waiting_writers_count--; // pisarz wychodzi z kolejki i wchodzi do biblioteki

        // pisarz korzysta z biblioteki
        // (tylko jeden pisarz moze przebywac w bibliotece, wedlug zasady: dwie osoby nie moga modyfikowac zasobow)
        active_number_of_writers = 1;

        printf("ReaderQ: %d WriterQ: %d [in: R:%d W:%d] -> Wchodzi pisarz o id: %d" "\n",
               number_of_readers - active_number_of_readers,
               number_of_writers - active_number_of_writers,
               active_number_of_readers,
               active_number_of_writers,
               *id
        );

        rval = pthread_mutex_unlock(&mutex); // wyjscie z sekcji krytycznej
        if(rval) {
            fprintf(stderr,"Error with pthread mutex unlock return: %d",rval);
            exit(EXIT_FAILURE);
        }
        // poczatek kodu wykonywanego przez pisarza
        usleep(250 * 1000);
        // koniec kodu wykonywanego przez pisarza

        rval = pthread_mutex_lock(&mutex); // wejscie do sekcji krytycznej
        if(rval) {
            fprintf(stderr,"Error with pthread mutex lock return: %d",rval);
            exit(EXIT_FAILURE);
        }
        active_number_of_writers = 0; // pisarz wychodzi z biblioteki (tylko jeden pisarz moze przebywac w bibliotece)

        printf("ReaderQ: %d WriterQ: %d [in: R:%d W:%d] -> Wychodzi pisarz o id: %d" "\n",
               number_of_readers - active_number_of_readers,
               number_of_writers - active_number_of_writers,
               active_number_of_readers,
               active_number_of_writers,
               *id
        );

        // poinformuj wszystkich pisarzy i czytelnikow z kolejki, ze biblioteka jest wolna
        // (na podstawie warunku w petli while czytelnikow i pisarzy, z kolejki zostanie wybrany odpowiedni watek)
        rval = pthread_cond_broadcast(&cond);
        if(rval) {
            fprintf(stderr,"Error with pthread cond broadcast return: %d",rval);
            exit(EXIT_FAILURE);
        }

        rval = pthread_mutex_unlock(&mutex); // wyjscie z sekcji krytycznej
        if(rval) {
            fprintf(stderr,"Error with pthread mutex unlock return: %d",rval);
            exit(EXIT_FAILURE);
        }
    }
    return NULL;
}

void *reader_function(void *arg) {
    int *id = (int*) arg;
    while (1) {
        rval = pthread_mutex_lock(&mutex); // wejscie do sekcji krytycznej
        if(rval) {
            fprintf(stderr,"Error with pthread mutex lock return: %d",rval);
            exit(EXIT_FAILURE);
        }
        // czetelnik bedzie czekal w kolejce, jezeli:
        // * istnieje pisarz, ktory pisze <- nie mozna modyfikowac zasobow i jednoczesnie z nich korzystac
        // * przynajmniej jeden pisarz czeka w kolejce <- bo to rozwiazanie jest writer-friendly
        while (active_number_of_writers > 0 || waiting_writers_count > 0) {
            pthread_cond_wait(&cond, &mutex);
        }

        active_number_of_readers++; // dopiero, gdy nie bedzie chetnego pisarza, do biblioteki wchodzi czytelnik

        printf("ReaderQ: %d WriterQ: %d [in: R:%d W:%d] -> Wchodzi czytelnik o id: %d" "\n",
               number_of_readers - active_number_of_readers,
               number_of_writers - active_number_of_writers,
               active_number_of_readers,
               active_number_of_writers,
               *id
        );

        rval = pthread_mutex_unlock(&mutex); // wyjscie z sekcji krytycznej
        if(rval) {
            fprintf(stderr,"Error with pthread mutex unlock return: %d",rval);
            exit(EXIT_FAILURE);
        }
        // poczatek kodu wykonywanego przez czytelnika
        usleep(250 * 1000);
        // koniec kodu wykonywanego przez czytelnika

        rval = pthread_mutex_lock(&mutex); // wejscie do sekcji krytycznej
        if(rval) {
            fprintf(stderr,"Error with pthread mutex lock return: %d",rval);
            exit(EXIT_FAILURE);
        }
        active_number_of_readers--; // czytelnik wychodzi z biblioteki

        printf("ReaderQ: %d WriterQ: %d [in: R:%d W:%d] -> Wychodzi czytelnik o id: %d" "\n",
               number_of_readers - active_number_of_readers,
               number_of_writers - active_number_of_writers,
               active_number_of_readers,
               active_number_of_writers,
               *id
        );

        if (active_number_of_readers == 0) { // jezeli w bibliotece nie bedzie czytelnikow (a moze byc ich wielu)
            pthread_cond_broadcast(&cond); // poinformuj wszystkich pisarzy i czytelnikow z kolejki, ze biblioteka jest wolna
        }

        rval = pthread_mutex_unlock(&mutex); // wyjscie z sekcji krytycznej
        if(rval) {
            fprintf(stderr,"Error with pthread mutex unlock return: %d",rval);
            exit(EXIT_FAILURE);
        }
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    number_of_writers = 0;
    number_of_readers = 0;

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
                if(number_of_writers < 1) {
                    fprintf(stderr,"%s","Pisarz musi byc przynajmniej jeden !");
                    exit(EXIT_FAILURE);
                }
                break;

            default:
                fprintf(stderr,"%s","Podano nieprawidlowe argumenty");
                exit(EXIT_FAILURE);
        }
    }
    waiting_writers_count = 0;
    active_number_of_readers = 0;
    active_number_of_writers = 0;

    rval = pthread_mutex_init(&mutex, NULL);
    if(rval) {
        fprintf(stderr,"Error with pthread mutex init return: %d",rval);
        exit(EXIT_FAILURE);
    }

    rval = pthread_cond_init(&cond, NULL);
    if(rval) {
        fprintf(stderr,"Error with pthread cond init return: %d",rval);
        exit(EXIT_FAILURE);
    }
    pthread_t *writer_threads = malloc(sizeof(pthread_t) * number_of_writers);
    pthread_t *reader_threads = malloc(sizeof(pthread_t) * number_of_readers);




    for (int i = 0; i < number_of_readers; i++) {
        int *index = malloc(sizeof(int));
        *index = i;
        rval = pthread_create(&reader_threads[i], NULL, reader_function, index);
        if(rval) {
            fprintf(stderr,"Error - pthread_create() reader threads return: %d\n",rval);
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < number_of_writers; i++) {
        int *index = malloc(sizeof(int));
        *index = i;
        rval = pthread_create(&writer_threads[i], NULL, writer_function, index);
        if(rval) {
            fprintf(stderr,"Error - pthread_create() writer threads return: %d\n",rval);
            exit(EXIT_FAILURE);
        }
    }

    Join_Pthread(number_of_readers,reader_threads);
    Join_Pthread(number_of_writers, writer_threads);


    free(writer_threads);
    writer_threads = NULL;

    free(reader_threads);
    reader_threads = NULL;

    rval = pthread_mutex_destroy(&mutex);
    if(rval) {
        fprintf(stderr,"Error with pthread mutex destroy return: %d", rval);
        exit(EXIT_FAILURE);
    }

    rval = pthread_cond_destroy(&cond);
    if(rval) {
        fprintf(stderr,"Error with pthread cond destroy return: %d", rval);
        exit(EXIT_FAILURE);
    }
    return 0;
}


void Join_Pthread(int number, const pthread_t *pthread) {
    int rval;
    for (int i = 0; i < number; i++) {
        void *index = NULL;
        rval = pthread_join(pthread[i], &index);
        if(rval)
        {
            fprintf(stderr,"Error - pthread_join() return: %d\n",rval);
            exit(EXIT_FAILURE);
        }
        free(index);
        index = NULL;
    }
}