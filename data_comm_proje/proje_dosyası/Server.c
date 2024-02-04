//BORA SEZER ÇETİN 2010205066
//KADİR ALBAYRAK   2010205043
//LEVET ŞEN        2110205078
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_CLIENTS 100
#define BUFFER_SZ 2048

static _Atomic unsigned int cli_count = 0;
static int uid = 10;

//istemcinin yapısı
typedef struct
{
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[32];
} client_t;

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void str_overwrite_stdout()
{
    printf("\r%s", "> ");
    fflush(stdout);
}

void str_trim_lf(char *arr, int length)
{
    int i;
    for (i = 0; i < length; i++)
    { 
        if (arr[i] == '\n')
        {
            arr[i] = '\0';
            break;
        }
    }
}

void print_client_addr(struct sockaddr_in addr)
{
    printf("%d.%d.%d.%d",
           addr.sin_addr.s_addr & 0xff,
           (addr.sin_addr.s_addr & 0xff00) >> 8,
           (addr.sin_addr.s_addr & 0xff0000) >> 16,
           (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

//kuyruğa istemci ekleme
void queue_add(client_t *cl)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (!clients[i])
        {
            clients[i] = cl;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

//kuyruktan istemci çıkarma
void queue_remove(int uid)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (clients[i]->uid == uid)
            {
                clients[i] = NULL;
                break;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

//gönderen hariç diğer istemcilere mesaj gönderen kısım
void mesaj_gonderme(char *s, int uid)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (clients[i]->uid != uid)
            {
                if (write(clients[i]->sockfd, s, strlen(s)) < 0)
                {
                    perror("HATA: yazma basarisiz oldu.");
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}
void send_client_list(int uid)
{
    char client_list[BUFFER_SZ] = "";
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            strcat(client_list, clients[i]->name);
            strcat(client_list, "\n");
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    mesaj_gonderme(client_list, uid); // istemci listesini belirlenen istemciye gönder
}
void mesaj_degistirme(char *message)
{
    // Gelen mesajı kopyala
    char original_message[BUFFER_SZ];
    strcpy(original_message, message);

    // Mesajın içeriği ve gönderen ismi için pointer oluşturma
    char *sender = strtok(message, ":");
    char *content = strtok(NULL, "");

    
    if (content != NULL && strlen(content) > 0)
    {
        int content_len = strlen(content);
        // Rastgele bir indeks seç
        int random_index = rand() % content_len;
        // Rastgele bir karakter seç ve içeriği değiştir
        char random_char = 'A' + (rand() % 26); // Rastgele bir harf
        content[random_index] = random_char;

        // Yeni mesajı birleştir
        sprintf(message, "%s:%s", sender, content);
    }

    // Değiştirilmiş ve orijinal mesajı yazdır
    printf("Değiştirilmiş Mesaj: %s\n", message);
    printf("Orijinal Mesaj: %s\n", original_message);
}

//istemci iletişim kısmı
void *handle_client(void *arg)
{
    char buff_out[BUFFER_SZ];
    char name[32];
    int leave_flag = 0;

    cli_count++;
    client_t *cli = (client_t *)arg;

    //kullanıcı adı
    if (recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 32 - 1)
    {
        printf("Kullanici adi girilmedi.\n");
        leave_flag = 1;
    }
    else
    {
        strcpy(cli->name, name);
        sprintf(buff_out, "%s oturuma katildi\n", cli->name);
        printf("%s", buff_out);
        mesaj_gonderme(buff_out, cli->uid);
    }

    bzero(buff_out, BUFFER_SZ);

    while (1)
    {
        if (leave_flag)
        {
            break;
        }

        int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
        if (receive > 0)
        {
            if (strlen(buff_out) > 0)
            {
                mesaj_degistirme(buff_out); // Gelen mesajı değiştir
                mesaj_gonderme(buff_out, cli->uid);

                str_trim_lf(buff_out, strlen(buff_out));
                printf("%s -> %s\n", buff_out, cli->name);
            }
        }
        else if (receive == 0 || strcmp(buff_out, "cikis") == 0)
        {
            sprintf(buff_out, "%s oturumdan ayrildi\n", cli->name);
            printf("%s", buff_out);
            mesaj_gonderme(buff_out, cli->uid);
            leave_flag = 1;
        }
        else
        {
            printf("HATA: -1\n");
            leave_flag = 1;
        }

        bzero(buff_out, BUFFER_SZ);
    }

    //istemciyi kuyruktan silme
    close(cli->sockfd);
    queue_remove(cli->uid);
    free(cli);
    cli_count--;
    pthread_detach(pthread_self());

    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Kullanilan Port: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *ip = "127.0.0.1";
    int port = atoi(argv[1]);
    int option = 1;
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    //soket ayarları
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(8080);

    
    signal(SIGPIPE, SIG_IGN);

    if (setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *)&option, sizeof(option)) < 0)
    {
        perror("HATA: setsockopt hatasi");
        return EXIT_FAILURE;
    }

    //soket bağlama
    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("HATA: Soket baglantisi basarisiz oldu");
        return EXIT_FAILURE;
    }

    
    if (listen(listenfd, 10) < 0)
    {
        perror("HATA: Soket hatasi");
        return EXIT_FAILURE;
    }

    printf("<<<<<<<<<<<<<<<OTURUMA HOS GELDINIZ>>>>>>>>>>>>>>>\n");

    while (1)
    {
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &clilen);

        //istemci sayısı kontrol eder (max)
        if ((cli_count + 1) == MAX_CLIENTS)
        {
            printf("Maksimum istemciye ulasildi. Reddedildi: ");
            print_client_addr(cli_addr);
            printf(":%d\n", cli_addr.sin_port);
            close(connfd);
            continue;
        }

        //istemci ayarları
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = cli_addr;
        cli->sockfd = connfd;
        cli->uid = uid++;

        
        queue_add(cli);
        pthread_create(&tid, NULL, &handle_client, (void *)cli);

        
        sleep(1);
    }

    return EXIT_SUCCESS;
}