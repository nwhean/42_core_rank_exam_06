#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>


// function prototypes
void	ft_putstr_fd(const char *s, int fd);
void	ft_error(const char *s);
void	ft_fatal();
void	*ft_memmove(void *dst, const void *src, size_t len);

int		setup_listener(int port);


/* Write string 's' to file descriptor 'fd' */
void	ft_putstr_fd(const char *s, int fd)
{
	write(fd, s, strlen(s));
}

/* Write error message 's' to stderr and exit */
void	ft_error(const char *s)
{
	ft_putstr_fd(s, 2);
	exit(1);
}

/* Write "Fatal error\n" to stderror and exit */
void	ft_fatal()
{
	ft_error("Fatal error\n");
}

/* Copies len bytes from string src to string dst. */
void	*ft_memmove(void *dst, const void *src, size_t len)
{
	char		*d;
	const char	*s;

	d = dst + len;
	s = src + len;
	while (len-- > 0)
	{
		*--d = *--s;
	}
	return (dst);
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

int	setup_listener(int port)
{
	int sockfd;
	struct sockaddr_in servaddr;

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1)
	   ft_fatal();	
	else
		printf("Socket successfully created..\n"); 
	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port);
  
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		ft_fatal();
	else
		printf("Socket successfully binded..\n");
	if (listen(sockfd, 10) != 0)
		ft_fatal();
	return sockfd;
}

int main(int argc, char **argv) {
	int	listener;
	int	connfd;
	socklen_t	len;
	struct sockaddr_in cli; 

	if (argc != 2)
		ft_error("Wrong number of arguments\n");
	listener = setup_listener(atoi(argv[1]));
	len = sizeof(cli);
	connfd = accept(listener, (struct sockaddr *)&cli, &len);
	if (connfd < 0)
		ft_fatal();
    else
        printf("server acccept the client...\n");
}

