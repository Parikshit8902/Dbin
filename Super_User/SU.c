//------------------------------------------------------------------------------------//
		              //DBIN SUPER USER Program//                    
//------------------------------------------------------------------------------------//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <libgen.h>
#include <ctype.h>

// Port Definitions
#define SU_IP_NU 8100
#define SU_IP_CR 8101
#define NU_SENDTO_SU 8102
#define SU_SENDTO_NU 8103
#define SU_SENDTO_CR 8104
#define FSEE_PORT 8108
#define FBACK_PORT 8111
#define TCP_FILE_TRANSFER_PORT 9000

#define MAX_CHUNK_SIZE 4096
#define MAX_IP_LENGTH 16
#define MAX_CMD_LENGTH 512
#define MAX_NODES 10
#define MAX_FILENAME_LENGTH 256
#define MAX_FILEPATH_LENGTH 512

// Global State 
char G_IP_TABLE[MAX_NODES + 2][MAX_IP_LENGTH];
int G_NUM_NODES_IN_TABLE = 0;
volatile bool G_EXIT_REQUEST = false;

// Structs for thread arguments
typedef struct { int nu_sock; int fsee_reply_sock; int fback_reply_sock; } listener_args;
typedef struct { char filename[MAX_FILENAME_LENGTH]; char sender_ip[MAX_IP_LENGTH]; } tcp_download_info;

// Function Prototypes
void trim_whitespace(char *str);
bool is_ip_in_table(const char* ip_to_check);
void execute_tcp_upload(const char* dest_ip, int port, const char* filepath);
void initiate_file_transfer(const char* dest_ip, int port, const char* filepath, const char* self_ip);
void execute_tcp_download(const char* source_ip, int port, const char* save_as_filename);
void broadcast_message(const char* message, int nu_port, int cr_port);
void* tcp_download_thread(void* arg);
void* listener_thread_func(void* arg);

// Utility Functions 
void trim_whitespace(char *str) 
{
  if (!str) return;
  char *end = str + strlen(str) - 1;
  while (end >= str && isspace((unsigned char)*end)) end--;
  *(end + 1) = '\0';
  char *start = str;
  while (*start && isspace((unsigned char)*start)) start++;
  memmove(str, start, strlen(start) + 1);
}

bool is_ip_in_table(const char* ip_to_check) 
{
  for (int i = 0; i < G_NUM_NODES_IN_TABLE; ++i) 
  {
    if (strcmp(G_IP_TABLE[i], ip_to_check) == 0) return true;
  }
  return false;
}

// TCP Transfer and Handshake Functions
void execute_tcp_upload(const char* dest_ip, int port, const char* filepath) 
{
  FILE* file = fopen(filepath, "rb");
  if (!file) 
  { 
    perror("fopen"); return; 
  }
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) 
  { 
    perror("TCP socket"); fclose(file); return; 
  }
  struct sockaddr_in dest_addr = { .sin_family = AF_INET, .sin_port = htons(port) };
  inet_pton(AF_INET, dest_ip, &dest_addr.sin_addr);

  if (connect(sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) 
  {
    perror("TCP connect"); fclose(file); close(sock); return;
  }
  char buffer[MAX_CHUNK_SIZE];
  size_t bytes_read;
  while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) 
  {
    if (send(sock, buffer, bytes_read, 0) < 0) 
    { 
      perror("TCP send"); 
      break; 
    }
  }
  fclose(file);
  close(sock);
  printf("File transfer complete.\n");
}

void initiate_file_transfer(const char* dest_ip, int port, const char* filepath, const char* self_ip) 
{
  struct stat file_stat;
  if (stat(filepath, &file_stat) < 0) 
  { 
    perror("stat"); 
    return; 
  }
    
  const char* filename = basename((char*)filepath);
  char command[512];
  snprintf(command, sizeof(command), "REQUEST_UPLOAD %s %lld %s", filename, (long long)file_stat.st_size, self_ip);

  int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in dest_addr = { .sin_family = AF_INET, .sin_port = htons(port) };
  inet_pton(AF_INET, dest_ip, &dest_addr.sin_addr);
  sendto(udp_sock, command, strlen(command), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
  close(udp_sock);
    
  printf("Upload request sent for '%s'. Waiting for peer to connect to TCP port %d...\n", filename, TCP_FILE_TRANSFER_PORT);
    
  // Delay for server to start its TCP listener
    sleep(1);
    
  // Immediately try to connect and upload the file via TCP
  execute_tcp_upload(dest_ip, TCP_FILE_TRANSFER_PORT, filepath);
}

void execute_tcp_download(const char* source_ip, int port, const char* save_as_filename) 
{
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) 
  { 
    perror("TCP socket"); 
    return; 
  }
  struct sockaddr_in source_addr = { .sin_family = AF_INET, .sin_port = htons(port) };
  inet_pton(AF_INET, source_ip, &source_addr.sin_addr);

  if (connect(sock, (struct sockaddr*)&source_addr, sizeof(source_addr)) < 0) 
  {
    perror("TCP connect for download"); close(sock); return;
  }
  mkdir("su_downloads", 0755);
  char save_path[MAX_FILEPATH_LENGTH];
  snprintf(save_path, sizeof(save_path), "su_downloads/%s", save_as_filename);
  FILE* file = fopen(save_path, "wb");
  if (!file) 
  { 
    perror("fopen for download"); close(sock); 
    return; 
  }

  char buffer[MAX_CHUNK_SIZE];
  ssize_t bytes_received;
  while ((bytes_received = recv(sock, buffer, sizeof(buffer), 0)) > 0) 
  {
    fwrite(buffer, 1, bytes_received, file);
  }
  fclose(file);
  close(sock);
  printf("File download complete. Saved as '%s'.\n", save_path);
}

void* tcp_download_thread(void* arg) 
{
  tcp_download_info* info = (tcp_download_info*)arg;
  int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in listen_addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(TCP_FILE_TRANSFER_PORT) };
  if (bind(listen_sock, (struct sockaddr*)&listen_addr, sizeof(listen_addr)) < 0) 
  {
    perror("TCP download bind"); close(listen_sock); free(info); 
    return NULL;
  }
  listen(listen_sock, 1);
    
  int data_sock = accept(listen_sock, NULL, NULL);
  close(listen_sock);
  if (data_sock < 0) 
  { 
    perror("TCP accept"); 
    free(info); 
    return NULL; 
  }

  mkdir("su_recv_from_nu", 0755);
  char save_path[MAX_FILEPATH_LENGTH];
  snprintf(save_path, sizeof(save_path), "su_recv_from_nu/%s", info->filename);
    
  FILE* file = fopen(save_path, "wb");
  if (!file) 
  { 
    perror("fopen download"); 
    close(data_sock); free(info); 
    return NULL; 
  }
    
  char buffer[MAX_CHUNK_SIZE];
  ssize_t bytes_received;
  while ((bytes_received = recv(data_sock, buffer, sizeof(buffer), 0)) > 0) 
  {
    fwrite(buffer, 1, bytes_received, file);
  }
  fclose(file);
  close(data_sock);
  printf("File '%s' received from %s.\n", info->filename, info->sender_ip);
  free(info);
  return NULL;
}

// Broadcast logic
void broadcast_message(const char* message, int nu_port, int cr_port) 
{
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) 
  { 
    perror("broadcast socket"); 
    return; 
  }
    
  int num_normal_users = G_NUM_NODES_IN_TABLE - 2;
  for (int i = 0; i < num_normal_users; ++i) 
  {
    struct sockaddr_in nu_addr = { .sin_family = AF_INET, .sin_port = htons(nu_port) };
    inet_pton(AF_INET, G_IP_TABLE[i], &nu_addr.sin_addr);
    sendto(sock, message, strlen(message), 0, (struct sockaddr*)&nu_addr, sizeof(nu_addr));
  }
  struct sockaddr_in cr_addr = { .sin_family = AF_INET, .sin_port = htons(cr_port) };
  inet_pton(AF_INET, G_IP_TABLE[num_normal_users], &cr_addr.sin_addr);
  sendto(sock, message, strlen(message), 0, (struct sockaddr*)&cr_addr, sizeof(cr_addr));
  close(sock);
}

// Listening logic
void* listener_thread_func(void* arg) 
{
  listener_args* args = (listener_args*)arg;
  fd_set read_fds;
  int max_fd = args->nu_sock;
  if (args->fsee_reply_sock > max_fd) max_fd = args->fsee_reply_sock;
  if (args->fback_reply_sock > max_fd) max_fd = args->fback_reply_sock;
    
  while (!G_EXIT_REQUEST) 
  {
    FD_ZERO(&read_fds);
    FD_SET(args->nu_sock, &read_fds);
    FD_SET(args->fsee_reply_sock, &read_fds);
    FD_SET(args->fback_reply_sock, &read_fds);

    struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
    select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

    if (FD_ISSET(args->nu_sock, &read_fds)) 
    {
      char buffer[MAX_CMD_LENGTH];
      ssize_t len = recvfrom(args->nu_sock, buffer, sizeof(buffer) - 1, 0, NULL, NULL);
      if (len > 0) 
      {
        buffer[len] = '\0';
        char filename[MAX_FILENAME_LENGTH], sender_ip[MAX_IP_LENGTH];
        long long filesize;
        if (sscanf(buffer, "REQUEST_UPLOAD %s %lld %s", filename, &filesize, sender_ip) == 3) 
        {
          tcp_download_info* info = malloc(sizeof(tcp_download_info));
          strncpy(info->filename, filename, sizeof(info->filename) - 1);
          strncpy(info->sender_ip, sender_ip, sizeof(info->sender_ip) - 1);
          pthread_t download_tid;
          pthread_create(&download_tid, NULL, tcp_download_thread, info);
          pthread_detach(download_tid);
        }
      }
    }
        
    if (FD_ISSET(args->fsee_reply_sock, &read_fds)) 
    {
      char buffer[MAX_CHUNK_SIZE];
      ssize_t len = recvfrom(args->fsee_reply_sock, buffer, sizeof(buffer) - 1, 0, NULL, NULL);
      if (len > 0) 
      {
        buffer[len] = '\0';
        printf("\n--- CR Reply (fsee) ---\n%s\n> ", buffer);
        fflush(stdout);
      }
    }
        
    if (FD_ISSET(args->fback_reply_sock, &read_fds)) 
    {
      char buffer[MAX_CMD_LENGTH];
      struct sockaddr_in sender_addr;
      socklen_t sender_len = sizeof(sender_addr);
      ssize_t len = recvfrom(args->fback_reply_sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&sender_addr, &sender_len);
      if (len > 0) 
      {
        buffer[len] = '\0';
        char cr_ip[MAX_IP_LENGTH], filename[MAX_FILENAME_LENGTH];
        int tcp_port;
        inet_ntop(AF_INET, &sender_addr.sin_addr, cr_ip, sizeof(cr_ip));

        if (sscanf(buffer, "READY_TO_SEND %s %d", filename, &tcp_port) == 2) 
        {
          execute_tcp_download(cr_ip, tcp_port, filename);
        } 
        else 
        {
          printf("\n--- CR Reply (fback) ---\n%s\n> ", buffer);
          fflush(stdout);
        }
      }
    }
  }
  return NULL;
}

//Main
int main() 
{
  printf("Running Super User.\n\n");
  int num_normal_users = 0;
  char input_buffer[MAX_CMD_LENGTH];

  printf("Enter the number of Normal Users (max %d): ", MAX_NODES);
  fgets(input_buffer, sizeof(input_buffer), stdin);
  num_normal_users = atoi(input_buffer);
  if (num_normal_users < 0 || num_normal_users > MAX_NODES) 
  {
    fprintf(stderr, "Invalid number of users.\n"); 
    return EXIT_FAILURE;
  }

  printf("Enter Normal User IP addresses:\n");
  for (int i = 0; i < num_normal_users; ++i) 
  {
    printf("  IP of NU %d: ", i + 1);
    fgets(input_buffer, sizeof(input_buffer), stdin);
    input_buffer[strcspn(input_buffer, "\n")] = 0;
    strncpy(G_IP_TABLE[i], input_buffer, MAX_IP_LENGTH - 1);
    G_IP_TABLE[i][MAX_IP_LENGTH - 1] = '\0';
  }

  printf("Enter Central Repository IP address: ");
  fgets(input_buffer, sizeof(input_buffer), stdin);
  input_buffer[strcspn(input_buffer, "\n")] = 0;
  strncpy(G_IP_TABLE[num_normal_users], input_buffer, MAX_IP_LENGTH - 1);
  G_IP_TABLE[num_normal_users][MAX_IP_LENGTH - 1] = '\0';

  printf("Enter this Super User machine's correct network IP: ");
  fgets(input_buffer, sizeof(input_buffer), stdin);
  input_buffer[strcspn(input_buffer, "\n")] = 0;
  strncpy(G_IP_TABLE[num_normal_users + 1], input_buffer, MAX_IP_LENGTH - 1);
  G_IP_TABLE[num_normal_users + 1][MAX_IP_LENGTH - 1] = '\0';
  printf("Super User IP has been set to: %s\n", G_IP_TABLE[num_normal_users + 1]);

  G_NUM_NODES_IN_TABLE = num_normal_users + 2;
  char* self_ip = G_IP_TABLE[num_normal_users + 1];
    
  char iptable_message[1024] = "IP Table:\n";
  for (int i = 0; i < G_NUM_NODES_IN_TABLE; ++i) 
  {
    strncat(iptable_message, G_IP_TABLE[i], sizeof(iptable_message) - strlen(iptable_message) - 1);
    strncat(iptable_message, "\n", sizeof(iptable_message) - strlen(iptable_message) - 1);
  }
  printf("\nBroadcasting IP table to all nodes...\n");
  broadcast_message(iptable_message, SU_IP_NU, SU_IP_CR);
  sleep(1);

  listener_args args;
  struct sockaddr_in nu_addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(NU_SENDTO_SU) };
  struct sockaddr_in fsee_addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(FSEE_PORT) };
  struct sockaddr_in fback_addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(FBACK_PORT) };

  args.nu_sock = socket(AF_INET, SOCK_DGRAM, 0);
  args.fsee_reply_sock = socket(AF_INET, SOCK_DGRAM, 0);
  args.fback_reply_sock = socket(AF_INET, SOCK_DGRAM, 0);
    
  int opt = 1;
  setsockopt(args.fsee_reply_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  setsockopt(args.fback_reply_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
  if (bind(args.nu_sock, (struct sockaddr*)&nu_addr, sizeof(nu_addr)) < 0) 
  { 
    perror("bind nu_sock"); 
    return EXIT_FAILURE; 
  }
    
  if (bind(args.fsee_reply_sock, (struct sockaddr*)&fsee_addr, sizeof(fsee_addr)) < 0) 
  { 
    perror("bind fsee_sock"); 
    return EXIT_FAILURE; 
  }

  if (bind(args.fback_reply_sock, (struct sockaddr*)&fback_addr, sizeof(fback_addr)) < 0) 
  { 
    perror("bind fback_sock"); 
    return EXIT_FAILURE; 
  }

  pthread_t listener_tid;
  pthread_create(&listener_tid, NULL, listener_thread_func, &args);

  printf("\nCommands: fnu, fdel, fsee, fback, cleardb, kall\n> ");
  while (!G_EXIT_REQUEST && fgets(input_buffer, sizeof(input_buffer), stdin)) 
  {
    input_buffer[strcspn(input_buffer, "\n")] = 0;
    char* saveptr;
    char* command = strtok_r(input_buffer, " ", &saveptr);
    if (!command || strlen(command) == 0) 
    { 
      printf("> "); 
      continue; 
    }

    char* ip = strtok_r(NULL, " ", &saveptr);
    char* file = strtok_r(NULL, "", &saveptr);

    if (ip && !is_ip_in_table(ip)) 
    {
      printf("Error: IP '%s' is not in the network.\n", ip);
    }
     
    else 
    {
      if (strcmp(command, "fnu") == 0) 
      {
        if (ip && file) initiate_file_transfer(ip, SU_SENDTO_NU, file, self_ip);
        else printf("Usage: fnu <nu_ip> <filepath>\n");
      } 
      else if (strcmp(command, "fdel") == 0) 
      {
        if (ip && file) initiate_file_transfer(ip, SU_SENDTO_CR, file, self_ip);
        else printf("Usage: fdel <cr_ip> <filepath>\n");
      } 
      else if (strcmp(command, "fsee") == 0 || strcmp(command, "cleardb") == 0 || strcmp(command, "fback") == 0) 
      {
        if (ip) 
        {
          if (strcmp(command, "fback") == 0 && !file) 
          {
            printf("Usage: fback <cr_ip> <filename>\n");
          } 
          else 
          {
            char msg[MAX_CMD_LENGTH];
            if (file) snprintf(msg, sizeof(msg), "%s %s", command, file);
            else snprintf(msg, sizeof(msg), "%s", command);
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            struct sockaddr_in cr_addr = { .sin_family = AF_INET, .sin_port = htons(SU_SENDTO_CR) };
            inet_pton(AF_INET, ip, &cr_addr.sin_addr);
            sendto(sock, msg, strlen(msg), 0, (struct sockaddr*)&cr_addr, sizeof(cr_addr));
            close(sock);
            printf("Request for '%s' sent to CR.\n", command);
          }
        } 
        else 
        {
          printf("Usage: %s <cr_ip> [args]\n", command);
        }
      } 
      else if (strcmp(command, "kall") == 0) 
      {
        printf("Sending termination signal...\n");
        broadcast_message("Connection Terminated.", SU_SENDTO_NU, SU_SENDTO_CR);
        G_EXIT_REQUEST = true;
      } 
      else 
      {
        printf("Unknown command: '%s'\n", command);
      }
    }
    if (!G_EXIT_REQUEST) printf("> ");
  }
    
  printf("Shutting down...\n");
  G_EXIT_REQUEST = true;
  pthread_join(listener_tid, NULL);
  close(args.nu_sock);
  close(args.fsee_reply_sock);
  close(args.fback_reply_sock);
    
  printf("Program terminated.\n");
  return 0;
}
