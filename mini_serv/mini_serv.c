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
	int				cap_in;
	int				cap_out;
	char			*buf_in;
	char			*buf_out;
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
char		*ft_strchr(const char *s, int c);

t_client	*client_new(int fd);
void		client_add(t_client *client);
void		client_remove(t_client *client);
void		client_clear(void);

int			setup_listener(int port);
int			get_max_fd(int listener);
void		wait_events(fd_set *rfds, fd_set *wfds, int listener);
void		manage_events(fd_set *rfds, fd_set *wfds, int listener);

void		handle_connection(int listener);
int			receive(t_client *client);
int			extract_one(int id, char *buffer, char delimiter);
int			extract_message(t_client *client, int is_open);
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

/* Return a pointer to the first occurrence of the character 'c' in 's'. */
char	*ft_strchr(const char *s, int c)
{
	char	ch;

	ch = (char) c;
	while (*s != ch && *s != '\0')
		++s;
	if (*s == ch)
		return ((char *) s);
	else
		return (NULL);
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
	new->cap_in = BUFFER_SIZE;
	new->cap_out = BUFFER_SIZE;
	new->offset_in = 0;
	new->offset_out = 0;
	new->buf_in = malloc(sizeof(char) * BUFFER_SIZE);
	new->buf_out = malloc(sizeof(char) * BUFFER_SIZE);
	if (!new->buf_in || !new->buf_out)
		ft_fatal();
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
	free(client->buf_in);
	free(client->buf_out);
	free(client);
}

/* Free all the memory allocated for g_clients linked list */
void	client_clear(void)
{
	t_client	*next;

	while (g_clients != NULL)
	{
		next = g_clients->next;
		client_remove(g_clients);
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
	t_client	*this;
	t_client	*next;

	if (FD_ISSET(listener, rfds))
		handle_connection(listener);
	this = g_clients;
	while (this != NULL)
	{
		next = this->next;
		if (FD_ISSET(this->fd, rfds))
		{
			if (!extract_message(this, receive(this)))
			{
				client_remove(this);
				this = NULL;
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

/* handle a new incoming connection */
void	handle_connection(int listener)
{
	int					connfd;
	socklen_t			len;
	struct sockaddr_in	cli;

	len = sizeof(cli);
	connfd = accept(listener, (struct sockaddr *)&cli, &len);
	if (connfd < 0)
		ft_fatal();
	client_add(client_new(connfd));
}

/* Calls recv to get message from socket buffer */
int	receive(t_client *client)
{
	ssize_t	byte;

	byte = recv(client->fd, client->buf_in + client->offset_in,
			client->cap_in - client->offset_in - 1, 0);
	if (byte <= 0)
		return (0);
	client->offset_in += byte;
	client->buf_in[client->offset_in] = '\0';
	if (client->offset_in == client->cap_in - 1)
	{
		client->cap_in *= 2;
		client->buf_in = realloc(client->buf_in, client->cap_in);
		if (!client->buf_in)
			ft_fatal();
	}
	return (1);
}

/* Extract one message from the client buffer, with a given delimiter */
int	extract_one(int id, char *buffer, char delimiter)
{
	char	*str;
	char	*end;
	int		len;

	end = ft_strchr(buffer, delimiter);
	if (!end)
		return (0);
	len = end - buffer + (delimiter != '\0');
	str = malloc(sizeof(char) * (len + 1));
	if (!str)
		ft_fatal();
	ft_memmove(str, buffer, len);
	str[len] = '\0';
	broadcast(id, str);
	free(str);
	return (len);
}

/* Calls recv and broadcast messages to other clients */
int	extract_message(t_client *client, int is_open)
{
	int		processed;
	int		len;

	processed = 0;
	len = 1;
	while (len != 0)
	{
		len = extract_one(client->id, client->buf_in + processed, '\n');
		processed += len;
	}
	if (!is_open)
	{
		len = extract_one(client->id, client->buf_in + processed, '\0');
		processed += len;
	}
	client->offset_in -= processed;
	ft_memmove(client->buf_in, client->buf_in + processed, client->offset_in);
	client->buf_in[client->offset_in] = '\0';
	return (is_open);
}

/* Put 'str' at the end of the output buffer for each client. */
void	broadcast(int source, char *str)
{
	t_client	*client;
	char		str_source[BUFFER_SIZE];
	int			len;

	if (source == -1)
		sprintf(str_source, "server: ");
	else
		sprintf(str_source, "client %d: ", source);
	len = strlen(str_source) + strlen(str);
	client = g_clients;
	while (client != NULL)
	{
		if (client->id != source)
		{
			while (client->offset_out + len > client->cap_out - 1)
			{
				client->cap_out *= 2;
				client->buf_out = realloc(client->buf_out, client->cap_out);
				if (!client->buf_out)
					ft_fatal();
			}
			strcat(client->buf_out, str_source);
			strcat(client->buf_out, str);
			client->offset_out += len;
		}
		client = client->next;
	}
}

/* Send data to socket */
int	transmit(t_client *client)
{
	ssize_t	byte;
	ssize_t	processed;

	byte = 1;
	processed = 0;
	while (client->offset_out && byte > 0)
	{
		byte = send(client->fd, client->buf_out + processed,
				client->offset_out - processed, 0);
		if (byte > 0)
			processed += byte;
	}
	client->offset_out -= processed;
	ft_memmove(client->buf_out, client->buf_out + processed,
		client->offset_out);
	client->buf_out[client->offset_out] = '\0';
	if (byte < 0)
		return (0);
	else
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
