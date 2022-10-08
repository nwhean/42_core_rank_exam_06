#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE	65536

typedef struct s_client
{
	int				id;
	int				fd;
	int				offset_in;
	int				offset_out;
	char			buf_in[BUFFER_SIZE];
	char			buf_out[BUFFER_SIZE];
	struct s_client	*next;
}	t_client;

// global variables
t_client	*g_clients;
int			g_id;

// function prototypes
void		ft_putstr_fd(const char *s, int fd);
void		ft_error(const char *s);
void		ft_fatal(void);
void		*ft_memmove(void *dst, const void *src, size_t len);

t_client	*client_new(int fd);
void		client_add(t_client *client);
void		client_remove(t_client *client);
void		client_clear(void);

int			setup_listener(int port);
int			get_max_fd(int listener);
void		wait_events(fd_set *rfds, fd_set *wfds, int listener);
void		manage_events(fd_set *rfds, fd_set *wfds, int listener);

int			extract_message(t_client *client);
void		broadcast(int source, char *str);
int			transmit(t_client *client);

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
void	ft_fatal(void)
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

/* Allocate memory and return a new t_client data structure */
t_client	*client_new(int fd)
{
	t_client	*new;

	new = malloc(sizeof(t_client));
	if (new == NULL)
		ft_fatal();
	new->id = g_id++;
	new->fd = fd;
	new->offset_in = 0;
	new->offset_out = 0;
	new->buf_in[0] = '\0';
	new->buf_out[0] = '\0';
	new->next = NULL;
	return (new);
}

/* Add a new t_client to g_clients linked list */
void	client_add(t_client *client)
{
	t_client	*this;
	char		buffer[BUFFER_SIZE];

	sprintf(buffer, "client %d just arrived\n", client->id);
	broadcast(-1, buffer);
	if (g_clients == NULL)
		g_clients = client;
	else
	{
		this = g_clients;
		while (this->next != NULL)
			this = this->next;
		this->next = client;
	}
}

/* Remove a client from g_clients linked list */
void	client_remove(t_client *client)
{
	t_client	*this;
	char		buffer[BUFFER_SIZE];

	if (g_clients == client)
		g_clients = client->next;
	else
	{
		this = g_clients;
		while (this->next != client)
			this = this->next;
		this->next = client->next;
	}
	sprintf(buffer, "client %d just left\n", client->id);
	broadcast(-1, buffer);
	close(client->fd);
	free(client);
}

/* Free all the memory allocated for g_clients linked list */
void	client_clear(void)
{
	t_client	*next;

	while (g_clients != NULL)
	{
		next = g_clients->next;
		close(g_clients->fd);
		free(g_clients);
		g_clients = next;
	}
}

int	setup_listener(int port)
{
	int					sockfd;
	struct sockaddr_in	servaddr;
	socklen_t			len;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
		ft_fatal();
	len = sizeof(servaddr);
	bzero(&servaddr, len);
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433);
	servaddr.sin_port = htons(port);
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, len)) != 0)
		ft_fatal();
	if (listen(sockfd, 10) != 0)
		ft_fatal();
	return (sockfd);
}

/* Return the largest file descriptor in use by listener and g_clients */
int	get_max_fd(int listener)
{
	int			retval;
	t_client	*this;

	retval = listener;
	this = g_clients;
	while (this != NULL)
	{
		if (this->fd > retval)
			retval = this->fd;
		this = this->next;
	}
	return (retval);
}

/* Call select and wait for specific events */
void	wait_events(fd_set *rfds, fd_set *wfds, int listener)
{
	t_client	*this;

	FD_ZERO(rfds);
	FD_ZERO(wfds);
	FD_SET(listener, rfds);
	this = g_clients;
	while (this != NULL)
	{
		FD_SET(this->fd, rfds);
		if (this->offset_out > 0)
			FD_SET(this->fd, wfds);
		this = this->next;
	}
	select(get_max_fd(listener) + 1, rfds, wfds, NULL, NULL);
}

/* Handle connection, read or write as necessary */
void	manage_events(fd_set *rfds, fd_set *wfds, int listener)
{
	int					connfd;
	socklen_t			len;
	struct sockaddr_in	cli;
	t_client			*this;
	t_client			*next;

	if (FD_ISSET(listener, rfds))
	{
		len = sizeof(cli);
		connfd = accept(listener, (struct sockaddr *)&cli, &len);
		if (connfd < 0)
			ft_fatal();
		client_add(client_new(connfd));
	}
	this = g_clients;
	while (this != NULL)
	{
		next = this->next;
		if (FD_ISSET(this->fd, rfds))
		{
			// recv from socket and extract messages
			// dummy operation to prevent infinite loop
			if (this->offset_in == 0)
			{
				if (!extract_message(this))
				{
					client_remove(this);
					this = NULL;
				}
			}
		}
		if (this && FD_ISSET(this->fd, wfds))
		{
			if (!transmit(this))
				client_remove(this);
		}
		this = next;
	}
}

/* Calls recv and broadcast messages to other clients */
int	extract_message(t_client *client)
{
	ssize_t	byte;
	char	buffer[BUFFER_SIZE];
	char	*end;
	int		processed;
	int		len;

	byte = recv(client->fd, client->buf_in + client->offset_in,
			BUFFER_SIZE - client->offset_in - 1, 0);
	if (byte <= 0)
		return (0);
	client->offset_in += byte;
	client->buf_in[client->offset_in] = '\0';
	processed = 0;
	end = strstr(client->buf_in + processed, "\n");
	while (end != NULL)
	{
		len = end - client->buf_in - processed + 1;
		ft_memmove(buffer, client->buf_in + processed, len);
		buffer[len] = '\0';
		broadcast(client->id, buffer);
		processed += len;
		end = strstr(client->buf_in + processed, "\n");
	}
	client->offset_in -= processed;
	ft_memmove(client->buf_in, client->buf_in + processed, client->offset_in);
	client->buf_in[client->offset_in] = '\0';
	return (1);
}

/* Put 'str' at the end of the output buffer for each client. */
void	broadcast(int source, char *str)
{
	char		buffer[BUFFER_SIZE];
	t_client	*client;
	int			len;

	if (source == -1)
		sprintf(buffer, "server: %s", str);
	else
		sprintf(buffer, "client %d: %s", source, str);
	len = strlen(buffer);
	client = g_clients;
	while (client != NULL)
	{
		if (client->id != source && client->offset_out + len < BUFFER_SIZE - 1)
		{
			strcat(client->buf_out, buffer);
			client->offset_out += len;
		}
		client = client->next;
	}
}

/* Send data to socket */
int	transmit(t_client *client)
{
	ssize_t	byte;

	byte = send(client->fd, client->buf_out, client->offset_out, 0);
	if (byte < 0)
		return (0);
	if (byte > 0)
	{
		client->offset_out -= byte;
		ft_memmove(client->buf_out, client->buf_out + byte, client->offset_out);
		client->buf_out[client->offset_out] = '\0';
	}
	return (1);
}

int	main(int argc, char **argv)
{
	int		listener;
	fd_set	rfds;
	fd_set	wfds;

	if (argc != 2)
		ft_error("Wrong number of arguments\n");
	listener = setup_listener(atoi(argv[1]));
	while (1)
	{
		wait_events(&rfds, &wfds, listener);
		manage_events(&rfds, &wfds, listener);
	}
	close(listener);
	client_clear();
}
