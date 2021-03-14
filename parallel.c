/*
A24. (8 puncte) Scrieti un program "#paralel" care se lanseaza sub forma:
     #paralel   com1   #,   com2   #,   ...   #,   comn   #endparalel
 (tot ce e dupa "#paralel" sunt argumentele lui), unde com1, ..., comn sunt
 comenzi externe (adica asociate unor fisiere executabile de pe disc) avand
 eventual si argumente in linia de comanda (deci comi este un sir de cuvinte)
 si lanseaza n+2 procese care se desfasoara in paralel:
  - cate un proces care executa fiecare dintre comi;
  - un proces de intrare care citeste caractere de la intrarea standard si 
     multiplica fiecare caracter in n copii, pe care le trimite in niste tuburi 
     conectate fiecare la intrarea standard a uneia din comi (deci fiecare din 
     comi are intrarea standard redirectata la cate un tub)
  - un proces de iesire care citeste caractere dintr-un tub si le scrie la 
     iesirea standard; la tubul respectiv sunt conectate iesirile standard ale
     proceselor comi.
 Schema de functionare va fi:
               /---> tubi1 ---> com1 ---\
             /                            \
     intrare    ........................    ---> tubo ---> iesire         
             \                            /
	       \---> tubin ---> comn ---/            
 Tuburile sunt fara nume si exista doar pe perioada executiei acestor procese.
 Procesul de intrare are SIGPIPE anihilat, pentru a nu se termina daca comi nu
 se termina toate in acelasi timp (si isi inchid tubul de intrare la citire).
 Pentru a nu crea confuzii, in scrierea comenzilor comi nu va aparea #.	       
 Aplicati programul "#paralel" pentru a crea generalizari ale comenzilor filtru
 in care in loc de un lant com1 | ... | comn avem un graf.
 */

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <wait.h>

#define MAX_COM 256
#define MAX_ARGC 32
#define BUFF_SIZE 256

void ignore(int n)
{
    signal(n, ignore);
}

int main(int argc, char** argv)
{
    char* nargv[MAX_COM][MAX_ARGC];
    int nargc;
    int pin[MAX_COM][2]; // tuburi pentru intrare
    int pout[2]; // tub de iesire

    int i = 1;
    int open_paralel = 1;
    int com_no = 0;
    
    while(i < argc && open_paralel != 0)
    {   
        if(com_no >= MAX_COM)
        {
            fprintf(stderr, "Prea multe comenzi (MAX %d)\n", MAX_COM);
            exit(1);
        }   

        nargc = 0;
		
        if(strcmp(argv[i], argv[0]) != 0) // cazul in care comanda este diferita de @paralel
        {   
            int endpar = 1;
            int hash = 1;

            while(i < argc && (hash = strcmp(argv[i], "@")) != 0 && (endpar = strcmp(argv[i], "@endparalel")) != 0) // adauga argumente pana se ajunge la @ sau la @endparalel
            {
                if(nargc + 1 >= MAX_ARGC) //+1 pentru NULL-ul de la sfarsit
                {
                    fprintf(stderr, "Prea multe argumente pentru o singura comanda (MAX %d)\n", MAX_ARGC - 1);
                    exit(1);
                }   
                nargv[com_no][nargc++] = argv[i++];
            }

            if(endpar == 0) // daca s-a intalnit un @endparalel, atunci decrementeaza numarul de @paralel fara pereche
            {
                open_paralel--;
                i++;
            }

            if(hash == 0)
            {
                i++;
            }
        }
        else // cazul in care comanda este @paralel
        {
            //adauga toate argumentele (inclusiv @ si @endparalel), pana cand singurul @paralel fara pereche (@endparalel) este cel initial
            do
            {
                if(strcmp(argv[i], argv[0]) == 0) // comanda este @paralel, incrementeaza numarul de @paralel fara pereche
                {
                    open_paralel++;
                }

                if(strcmp(argv[i], "@endparalel") == 0) // comanda este @endparalel, decrementeaza numarul de @paralel fara pereche
                {
                    open_paralel--;
                }

                if(nargc + 1 >= MAX_ARGC)
                {
                    fprintf(stderr, "Prea multe argumente pentru o singura comanda (MAX %d)\n", MAX_ARGC - 1);
                    exit(1);
                }

                nargv[com_no][nargc++] = argv[i++];                
            }
            while(i < argc && open_paralel > 1);
        }

		if (nargc > 0)
		{
			nargv[com_no][nargc] = NULL;

			com_no++;
		}
    }
    
    // printf("Comenzi: %d\n", com_no);
    // for(i = 0; i < com_no; i++)
    // {
    //     int j = 0;
    //     while(nargv[i][j])
    //     {
    //         printf("%s ", nargv[i][j]);
    //         j++;
    //     }
    //     printf("\n");
    // }

    // daca nu a fost data nicio comanda sau
    // numarul de @paralel este diferit de numarul de @endparalel
    if(com_no == 0 || open_paralel > 0)
    {
        fprintf(stderr, "Utilizare: %s com1 args1 [ @   ...   @ comn argsn ] @endparalel\n", argv[0]);
        exit(1);
    }

    if(pipe(pout) == -1)
    {
        perror("pipe out");
        exit(1);
    }

    for(i = 0; i < com_no; i++)
    {
        pipe(pin[i]); // creaza un tub pentru proces

        if(fork() == 0) // proces fiu
        {
            // schimba intrarea
            close(0);
            dup(pin[i][0]);
            
            // schimba iesirea
            close(1);
            dup(pout[1]);
            
            // inchide descriptorii neutilizati
            int j;
            for(j = 0; j <= i; j++)
            {
                close(pin[j][0]);
                close(pin[j][1]);
            }
            
            close(pout[0]);
            close(pout[1]);

            // inlocuieste procesul fiu cu un alt proces
            execv(nargv[i][0], nargv[i]);
            perror(nargv[i][0]);
            exit(1);
        }
    }

    if(fork()) // procesul tata citeste de la stdin si scrie in tuburile in
    {
        signal(SIGPIPE, SIG_IGN);
        
        // inchide descriptorii neutilizati
        for(i = 0; i < com_no; i++)
        {
            close(pin[i][0]);
        }
        
        close(pout[0]);
        close(pout[1]);

        char buff[BUFF_SIZE];
        ssize_t read_len;

        // citeste buff_size caractere de la stdin
        while((read_len = read(0, buff, BUFF_SIZE)) != 0)
        {
            if(read_len == -1)
            {
                perror("read");
                for(i = 0; i < com_no; i++)
                {
                    close(pin[i][1]);
                }
                exit(1);
            }
        
            // scrie read_len caractere din buff in fiecare tub
            for(i = 0; i < com_no; i++)
            {
                errno = 0;
                if(write(pin[i][1], buff, read_len) == -1)
                {
           
                    if(errno == EPIPE) //in caz ca incercam sa scriem intr-un pipe care a ramas fara cititori
                    {
                    	continue; //modificare
                       //exit(1);
                    }
                    perror("write");
                    for(i = 0; i < com_no; i++)
                    {
                        close(pin[i][1]);
                    }
                    exit(1);
                }
            }
        }
        // inchide si descriptorii pentru scriere
        for(i = 0; i < com_no; i++)
        {
            close(pin[i][1]);
        }

        for(i = 0; i < com_no + 1; i++)
        {
            wait(NULL);
        }
    }
    else // procesul fiu citeste din tubul out si scrie la stdout
    {
        // inchide descriptorii neutilizati
        for(i = 0; i < com_no; i++)
        {
            close(pin[i][0]);
            close(pin[i][1]);
        }
        
        close(pout[1]);

        char buff[BUFF_SIZE];
        ssize_t read_len = -1;

        while(read_len != 0)
        {
            if((read_len = read(pout[0], buff, BUFF_SIZE)) == -1)
            {
                perror("read");
                close(pout[0]);
                exit(1);
            }

            if(read_len != 0)
            {
                if(write(1, buff, read_len) == -1)
                {
                    perror("write");
                    close(pout[0]);
                    exit(1);
                }
            }
        }
        
        // inchide si descriptorul pentru citire
        close(pout[0]);
    }

    return 0;
}
