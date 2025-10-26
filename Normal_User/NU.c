//------------------------------------------------------------------------------------//
	              		//DBIN NORMAL USER PROGRAM//
//------------------------------------------------------------------------------------//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <libgen.h>
#include <ctype.h>

// Port Definitions 
#define SU_IP_NU 8100
#define NU_SENDTO_SU 8102
#define SU_SENDTO_NU 8103
#define NU_SENDTO_NU 8105
#define NU_RECVFROM_NU 8106
#define NU_SENDTO_CR 8107
#define CR_REPLY_PORT 8113
#define TCP_FILE_TRANSFER_PORT 9000

#define MAX_CHUNK_SIZE 4096
#define MAX_IP_LENGTH 16
#define MAX_CMD_LENGTH 512
#define MAX_NODES 10
#define MAX_FILENAME_LENGTH 256
#define MAX_FILEPATH_LENGTH 512

// Global Variables 
volatile bool G_EXIT_REQUEST = false;
char G_IP_TABLE[MAX_NODES + 2][MAX_IP_LENGTH];
int G_NUM_NODES_IN_TABLE = 0;

// Structs for thread arguments
typedef struct { int su_sock; int nu_sock; int cr_reply_sock; } listener_args;
typedef struct { char filename[MAX_FILENAME_LENGTH]; char sender_ip[MAX_IP_LENGTH]; } tcp_download_info;

// Function Prototypes
void trim_whitespace(char *str);
void get_self_ip(char* buffer, size_t buffer_size);
void parse_and_store_ip_table(const char* buffer);
bool is_ip_in_table(const char* ip_to_check);
void execute_tcp_upload(const char* dest_ip, int port, const char* filepath);
void initiate_file_transfer(const char* dest_ip, int port, const char* filepath, const char* self_ip);
void execute_tcp_download(const char* source_ip, int port, const char* save_as_filename);
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

void get_self_ip(char* ip_buffer, size_t buffer_size) 
{
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) 
  { 
    strncpy(ip_buffer, "127.0.0.1", buffer_size); 
    return; 
  }
  struct sockaddr_in dummy_addr = { .sin_family = AF_INET, .sin_addr.s_addr = inet_addr("8.8.8.8"), .sin_port = htons(53) };
  if (connect(sock, (const struct sockaddr*)&dummy_addr, sizeof(dummy_addr)) < 0) 
  {
    close(sock); strncpy(ip_buffer, "127.0.0.1", buffer_size); 
    return;
  }
  struct sockaddr_in local_addr;
  socklen_t addr_len = sizeof(local_addr);
  getsockname(sock, (struct sockaddr*)&local_addr, &addr_len);
  inet_ntop(AF_INET, &local_addr.sin_addr, ip_buffer, buffer_size);
  close(sock);
}

void parse_and_store_ip_table(const char* buffer) 
{
  char temp_buffer[1024];
  strncpy(temp_buffer, buffer, sizeof(temp_buffer));
  G_NUM_NODES_IN_TABLE = 0;
  strtok(temp_buffer, "\n");
  char* line = strtok(NULL, "\n");
  while (line != NULL && G_NUM_NODES_IN_TABLE < (MAX_NODES + 2)) 
  {
    trim_whitespace(line);
    if (strlen(line) > 0) 
    {
      strncpy(G_IP_TABLE[G_NUM_NODES_IN_TABLE], line, MAX_IP_LENGTH - 1);
      G_IP_TABLE[G_NUM_NODES_IN_TABLE][MAX_IP_LENGTH - 1] = '\0';
      G_NUM_NODES_IN_TABLE++;
    }
    line = strtok(NULL, "\n");
  }
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
    perror("fopen"); 
    return; 
  }
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) 
  { 
    perror("TCP socket"); 
    fclose(file); 
    return; 
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
    
  sleep(1);
    
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
  mkdir("nu_downloads", 0755);
  char save_path[MAX_FILEPATH_LENGTH];
  snprintf(save_path, sizeof(save_path), "nu_downloads/%s", save_as_filename);
  FILE* file = fopen(save_path, "wb");
  if (!file) 
  { 
    perror("fopen for download"); 
    close(sock); 
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
    perror("TCP download bind"); close(listen_sock); free(info); return NULL;
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

  // Differentiate save directory based on sender
  bool from_su = false;
  if (strcmp(info->sender_ip, G_IP_TABLE[G_NUM_NODES_IN_TABLE - 1]) == 0) from_su = true;

  const char* save_dir = from_su ? "nu_recv_from_su" : "nu_recv_from_nu";
  mkdir(save_dir, 0755);

  char save_path[MAX_FILEPATH_LENGTH];
  snprintf(save_path, sizeof(save_path), "%s/%s", save_dir, info->filename);
    
  FILE* file = fopen(save_path, "wb");
  if (!file) 
  { 
    perror("fopen download"); 
    close(data_sock); 
    free(info); 
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

// Listener logic
void* listener_thread_func(void* arg) 
{
  listener_args* args = (listener_args*)arg;
  fd_set read_fds;
  int max_fd = args->su_sock > args->nu_sock ? args->su_sock : args->nu_sock;
  max_fd = args->cr_reply_sock > max_fd ? args->cr_reply_sock : max_fd;

  while (!G_EXIT_REQUEST) 
  {
    FD_ZERO(&read_fds);
    FD_SET(args->su_sock, &read_fds);
    FD_SET(args->nu_sock, &read_fds);
    FD_SET(args->cr_reply_sock, &read_fds);

    struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
    select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

    if (FD_ISSET(args->su_sock, &read_fds) || FD_ISSET(args->nu_sock, &read_fds)) 
    {
      int active_sock = FD_ISSET(args->su_sock, &read_fds) ? args->su_sock : args->nu_sock;
      char buffer[MAX_CMD_LENGTH];
      ssize_t len = recvfrom(active_sock, buffer, sizeof(buffer) - 1, 0, NULL, NULL);
      if (len > 0) 
      {
        buffer[len] = '\0';
        if (strncmp(buffer, "Connection Terminated.", 22) == 0) 
        {
          printf("\nTermination signal received. Shutting down.\n");
          fflush(stdout); _exit(0);
        }
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
        
    if (FD_ISSET(args->cr_reply_sock, &read_fds)) 
    {
      char buffer[MAX_CHUNK_SIZE];
      struct sockaddr_in sender_addr;
      socklen_t sender_len = sizeof(sender_addr);
      ssize_t len = recvfrom(args->cr_reply_sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&sender_addr, &sender_len);
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
          printf("\n--- CR Reply ---\n%s\n> ", buffer);
          fflush(stdout);
        }
      }
    }
  }
  return NULL;
}

// Main
int main() 
{
  printf("Running Normal User.\n");
  char iptable_buffer[1024];
  int ip_sock = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in listen_addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(SU_IP_NU) };
  if (bind(ip_sock, (struct sockaddr*)&listen_addr, sizeof(listen_addr)) < 0) 
  { 
    perror("bind IP table"); 
    return EXIT_FAILURE; 
  }
  printf("Waiting for IP table...\n");
  ssize_t len = recvfrom(ip_sock, iptable_buffer, sizeof(iptable_buffer) - 1, 0, NULL, NULL);
  close(ip_sock);
  if (len < 0) 
  { 
    perror("recvfrom IP table"); 
    return EXIT_FAILURE; 
  }
  printf("IP table received from Super User.\n");
  iptable_buffer[len] = '\0';
  parse_and_store_ip_table(iptable_buffer);

  char self_ip[MAX_IP_LENGTH];
  get_self_ip(self_ip, sizeof(self_ip));

  listener_args args;
  struct sockaddr_in su_listen_addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(SU_SENDTO_NU) };
  struct sockaddr_in nu_listen_addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(NU_RECVFROM_NU) };
  struct sockaddr_in cr_reply_addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(CR_REPLY_PORT) };

  args.su_sock = socket(AF_INET, SOCK_DGRAM, 0);
  args.nu_sock = socket(AF_INET, SOCK_DGRAM, 0);
  args.cr_reply_sock = socket(AF_INET, SOCK_DGRAM, 0);
    
  int opt = 1;
  setsockopt(args.cr_reply_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  if (bind(args.su_sock, (struct sockaddr*)&su_listen_addr, sizeof(su_listen_addr)) < 0) 
  { 
    perror("bind su_sock"); 
    return EXIT_FAILURE; 
  }
  
  if (bind(args.nu_sock, (struct sockaddr*)&nu_listen_addr, sizeof(nu_listen_addr)) < 0) 
  { 
    perror("bind nu_sock"); 
    return EXIT_FAILURE; 
  }
  
  if (bind(args.cr_reply_sock, (struct sockaddr*)&cr_reply_addr, sizeof(cr_reply_addr)) < 0) 
  { 
    perror("bind cr_reply_sock"); 
    return EXIT_FAILURE; 
  }

  pthread_t listener_tid;
  pthread_create(&listener_tid, NULL, listener_thread_func, &args);

  char line[MAX_CMD_LENGTH];
  printf("\nCommands: fsu, fnu, fdel, seemyfiles, fback, exit\n> ");
  while (!G_EXIT_REQUEST && fgets(line, sizeof(line), stdin)) 
  {
    line[strcspn(line, "\n")] = 0;
    char* saveptr;
    char* command = strtok_r(line, " ", &saveptr);
    if (!command) 
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
      if (strcmp(command, "fsu") == 0 || strcmp(command, "fnu") == 0 || strcmp(command, "fdel") == 0) 
      {
        if (ip && file) 
        {
          int dest_port = 0;
          if (strcmp(command, "fsu") == 0) dest_port = NU_SENDTO_SU;
          if (strcmp(command, "fnu") == 0) dest_port = NU_SENDTO_NU;
          if (strcmp(command, "fdel") == 0) dest_port = NU_SENDTO_CR;
          initiate_file_transfer(ip, dest_port, file, self_ip);
        } 
        else printf("Usage: %s <dest_ip> <filepath>\n", command);
      } 
      
      else if (strcmp(command, "seemyfiles") == 0 || strcmp(command, "fback") == 0) 
      {
        if (ip) 
        {
          if (strcmp(command, "fback") == 0 && !file) printf("Usage: fback <cr_ip> <filename>\n");
          else 
          {
            char msg[MAX_CMD_LENGTH];
            if (file) snprintf(msg, sizeof(msg), "%s %s", command, file);
            else snprintf(msg, sizeof(msg), "%s", command);
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            struct sockaddr_in cr_addr = { .sin_family = AF_INET, .sin_port = htons(NU_SENDTO_CR) };
            inet_pton(AF_INET, ip, &cr_addr.sin_addr);
            sendto(sock, msg, strlen(msg), 0, (struct sockaddr*)&cr_addr, sizeof(cr_addr));
            close(sock);
            printf("Request for '%s' sent to CR.\n", command);
          }
        } 
        else printf("Usage: %s <cr_ip> [filename]\n", command);
      } 
      
      else if (strcmp(command, "exit") == 0) 
      {
        G_EXIT_REQUEST = true;
      } 
      
      else printf("Unknown command: '%s'\n", command);
    }
    if (!G_EXIT_REQUEST) printf("> ");
  }
    
  printf("Shutting down...\n");
  G_EXIT_REQUEST = true;
  pthread_join(listener_tid, NULL);
  close(args.su_sock);
  close(args.nu_sock);
  close(args.cr_reply_sock);
  printf("Program terminated.\n");
  return 0;
}
