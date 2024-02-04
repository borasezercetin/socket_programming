//BORA SEZER ÇETİN 2010205066
//KADİR ALBAYRAK   2010205043
//LEVET ŞEN        2110205078
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LENGTH 2048

//değikenler
volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[32];

void str_overwrite_stdout()
{
    printf("%s", "> ");
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

void catch_ctrl_c_and_exit(int sig)
{
    flag = 1;
}

//1.Metot
// Simple Parity Check
int simple_parity_check(char *data, int length)
{
    int parity = 0;
    for (int i = 0; i < length; ++i)
    {
        parity ^= data[i] & 1;
    }
    return parity;
}

//2.Metot
// Cyclic Redundancy Check (CRC)
unsigned int crc32b(unsigned char *message, int length)
{
    unsigned int crc = 0xFFFFFFFF;
    for (int i = 0; i < length; ++i)
    {
        crc = crc ^ message[i];
        for (int j = 0; j < 8; ++j)
        {
            crc = (crc >> 1) ^ ((crc & 1) * 0xEDB88320);
        }
    }
    return crc;
}

void send_msg_handler()
{
    char message[LENGTH] = {};
    char buffer[LENGTH + 32] = {};

    while (1)
    {
        str_overwrite_stdout();
        fgets(message, LENGTH, stdin);
        str_trim_lf(message, LENGTH);

        if (strcmp(message, "exit") == 0)
        {
            break;
        }
        else
        {
            int parity = simple_parity_check(message, strlen(message));
            unsigned int crc = crc32b((unsigned char *)message, strlen(message));
            
            printf("Metotlar:\n");
            printf("Simple Party Check: %d\n",parity);
            printf("Cyclic Redundancy Check: %u\n",crc);

            sprintf(buffer, "%s: %s\n", name, message);
            send(sockfd, buffer, strlen(buffer), 0);
        }

        bzero(message, LENGTH);
        bzero(buffer, LENGTH + 32);
    }
    catch_ctrl_c_and_exit(2);
}

void recv_msg_handler()
{
    char message[LENGTH] = {};
    while (1)
    {
        int receive = recv(sockfd, message, LENGTH, 0);
        if (receive > 0)
        {
            printf("%s", message);
            str_overwrite_stdout();
        }
        else if (receive == 0)
        {
            break;
        }
        else
        {
            
        }
        memset(message, 0, sizeof(message));
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *ip = "127.0.0.1";
    int port = atoi(argv[1]);

    signal(SIGINT, catch_ctrl_c_and_exit);

    printf("Lutfen kullanici adinizi giriniz: ");
    fgets(name, 32, stdin);
    str_trim_lf(name, strlen(name));

    if (strlen(name) > 32 || strlen(name) < 3)
    {
        printf("Kullanici adiniz 30 karakterden az 2 karakterden fazla olmalidir.\n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;

    /* soket kodları */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    // sunucu bağlantısı
    int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err == -1)
    {
        printf("HATA: baglanti\n");
        return EXIT_FAILURE;
    }

    // kullanıcı adı gönderme
    send(sockfd, name, 32, 0);

    printf("<<<<<<<<<<<<<<<OTURUMA HOS GELDINIZ>>>>>>>>>>>>>>>\n");

    pthread_t send_msg_thread;
    if (pthread_create(&send_msg_thread, NULL, (void *)send_msg_handler, NULL) != 0)
    {
        printf("HATA: pthread\n");
        return EXIT_FAILURE;
    }

    pthread_t recv_msg_thread;
    if (pthread_create(&recv_msg_thread, NULL, (void *)recv_msg_handler, NULL) != 0)
    {
        printf("HATA: pthread\n");
        return EXIT_FAILURE;
    }

    while (1)
    {
        if (flag)
        {
            printf("\nHOSCAKALIN\n");
            break;
        }
    }

    close(sockfd);

    return EXIT_SUCCESS;
}