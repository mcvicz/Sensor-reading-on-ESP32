#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h> 
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/wait.h>

typedef void Sigfunc(int);

//na podstawie R.Stevens unp.h

#ifndef MAXLINE
#define MAXLINE 1024
#endif // !MAXLINE

void Getaddrinfo(const char* restrict node, const char* restrict service, const struct addrinfo* restrict hints, struct addrinfo** restrict res)
{
	if (getaddrinfo(node, service, hints, res) != 0)
	{
		perror("Blad - funkcja getaddrinfo()");
		exit(1);
	}
}

Sigfunc* Signal(int signo, Sigfunc* func)
{
	struct sigaction act, oact;

	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	if (signo == SIGALRM)
	{
		act.sa_flags |= SA_RESTART;
	}

	if (sigaction(signo, &act, &oact) < 0)
	{
		return SIG_ERR;
	}
	return oact.sa_handler;
}

//funkcja pomocnicza do Readline()
//ZWRACANE WARTOSCI: (1)->oczyt zakonczony pomyslnie, (0)->osiagnieto koniec pliku, (-1)->blad odczytu
static ssize_t my_read(int _fd, char* _dstbuff)
{
	static long int _read_cnt = 0;		//liczba odczytanych danych
	static char* _read_ptr;				//wskaznik na aktualna pozycje w buforze _read_buf
	static char _read_buf[MAXLINE];		//bufor odczytanych danych

	if (_read_cnt <= 0)	//jesli nie odczytano zadnych danych
	{
	_again:
		//ODCZYT
		_read_cnt = read(_fd, _read_buf, sizeof(_read_buf));

		//SPRAWDZENIE POPRAWNOSCI ODCZYTU
		if (_read_cnt < 0)
		{
			//OBSLUGA BLEDOW
			if (errno == EINTR)		//przerwanie przez sygnal
			{
				goto _again;
			}
			else					//inne bledy
			{
				perror("Blad - funkcja my_read()");
				return(-1);
			}
		}//OBSLUGA KONCA PLIKU
		else if (_read_cnt == 0)
		{
			return(0);
		}
		_read_ptr = _read_buf;
	}

	_read_cnt--;
	*_dstbuff = *_read_ptr++;
	return(1);
}

//funkcja odczytuje linie znak po znaku
//ZWRACANE WARTOSCI: (_n)->liczba odczytanych znakow,(0)->koniec pliku, zadna linia nie odczytana,(-1)->blad odczytu
ssize_t Readline(int _fd, void* _dstbff, size_t _maxlen)
{
	ssize_t _rc;			//wynik zwracany przez my_read
	char _singleChar;		//pojedynczy odczytany znak
	char* _ptr = _dstbff;	//wskaznik pomocniczy do _dstbuff

	int _n;					//licznik iteracji petli
	for (_n = 1; _n < _maxlen; _n++)
	{
		//odczytanie jednego znaku
		_rc = my_read(_fd, &_singleChar);

		
		if (_rc == 1)		//odczyt powiod sie
		{
			//kopiowanie odczytanego znaku do bufora pomocniczego i postinkrementacja wskaznika
			*_ptr++ = _singleChar;
			//jesli odczytany znak to \0, to petla zostaje przerwana a linia konczy sie znakiem nowego wiersza
			if (_singleChar == '\n') break;
		}
		else if (_rc == 0) //osiagnieto koniec pliku
		{
			if (_n == 1) return(0);	//jesli nic nie odczytano zwroc 0
			else break;				//jesli cos odczytano przerwij petle
		}
		else 				//odczyt nie powiod sie
		{
			perror("Blad - funkcja Readline()\n\r");
			return(-1);
		}
	}

	*_ptr = '\0';		//dodaj na koncu koniec lini
	return(_n);			//zwroc liczbe odczytanyc znakow (bez \0)
}

//funkcja odczytuje _n bajtow danych
//_fd		- deskryptor
//_dstbuff	- bufor odbierajacy
//_n		- rozmiar odbiearnych danych
size_t Readn(int _fd, void* _dstbuff, size_t _n)
{
	size_t _n_left = _n;	//ilosc bajtow pozostalych do odebarnia
	ssize_t _n_read = 0;	//ilosc bajtow odebranych

	while (_n_left > 0)
	{
		//ODCZYT
		_n_read = read(_fd, _dstbuff, _n_left);

		//OBSLUGA BLEDOW
		if (_n_read < 0)
		{
			if (errno == EINTR)	//jesli blad wystapil przez przerwanie sygnalem rozpoacznij od nowa	
			{
				_n_read = 0;
			}
			else				//jesli wystapil inny blad zakoncz program
			{
				perror("Blad - funkcja read()\n");
				exit(1);
			}
		}
		else if (_n_read == 0)	//jesli to koniec pliku
		{
			break;
		}

		_n_left -= (size_t)_n_read;
		_dstbuff += _n_read;
	}
	return (_n - _n_left);
}

//funkcja przesyla _n bajtow danych
//_fd		- deskryptor
//_dstbuff	- bufor przesylowy
//_n		- rozmiar przesylanych danych
ssize_t Writen(int _fd, const void* _dstbuff, size_t _n)
{
	size_t _n_left = _n;		//ilosc bajtow pozostalych do przeslania
	ssize_t _n_written = 0;		//ilosc bajtow przeslanych

	while (_n_left > 0)
	{
		//ZAPIS
		_n_written = write(_fd, _dstbuff, _n_left);		//przeslanie i zapisanie liczby pomyslnie przeslanych bajtow

		//OBSLUGA BLEDOW
		if (_n_written <= 0)
		{
			if (errno == EINTR) //jesli blad wystapil przez przerwanie sygnalem rozpoacznij od nowa
			{
				_n_written = 0;
			}
			else				//jesli wystapil inny blad zakoncz program
			{
				perror("Blad - funkcja write()\n");
				exit(1);
			}
		}

		_n_left -= (size_t) _n_written;		//zaktualizowanie pozostalych do przeslania bajtow
		_dstbuff += _n_written;		//przesuniecie wskaznika na kolejny element bufora
	}
	return _n_written;
}

//funkcja tworzy proces potomny, zwraca pid procesu potomnego w przodku, zwraca 0 w potomku
pid_t Fork(void)
{
	pid_t _pid;

	if ((_pid = fork()) < 0)
	{
		perror("Blad - funkcja fork()\n");
		exit(1);
	}

	return _pid;
}

//funkcja sprawdzajaca podanie argumentu
void check_ip_arg(int _argc)
{
	if (_argc != 2)
	{
		perror("Nie podano adresu IP serwera \n");
		exit(1);
	}
}

//funkcja tworzaca gniazdo ze sprwdzeniem bledu
int Socket(int _family, int _type, int _protocol)
{
	int _sockfd;
	if ((_sockfd = socket(_family, _type, _protocol)) < 0)
	{
		perror("Blad w tworzeniu gniazda - funkcja socket() \n");
		exit(1);
	}
	return _sockfd;
}

//funkcja konwertuje adres IP z tekstu na liczbe binarna
void Inet_pton(int _family, const char* restrict _src, void* restrict _dst)
{
	if (inet_pton(_family, _src, _dst) <= 0)
	{
		perror("Blad - funkcja inet_pton() \n");
		exit(1);
	}
}

//funckja konwertuje adres IP z liczby binarnej na tekst
const char* Inet_ntop(int _family, const void* restrict _src, char* restrict _dst, socklen_t _size)
{
	if ((inet_ntop(_family, _src, _dst, _size)) == NULL)
	{
		printf("Blad - funkcja inet_ntop() \n");
		exit(1);
	}
	return _dst;
}

//funkcja przydzielajaca strukture adresowa do gniazda
void Bind(int _sockfd, const struct sockaddr* _addr, socklen_t _addrlen)
{
	if ((bind(_sockfd, _addr, _addrlen)) < 0)
	{
		perror("Blad - funkcja bind() \n");
		exit(1);
	}
}

//funkcja rozpoczyna nasluchiwanie na gniezdzie
void Listen(int _sockfd, int _backlog)
{
	if ((listen(_sockfd, _backlog)) < 0)
	{
		perror("Blad - funkcja listen() \n");
		exit(1);
	}
}

//funkcja inicjuje polaczenie na gniezdzie
void Connect(int _sockfd, const struct sockaddr* _dstaddr, socklen_t _dstaddrlen)
{
	if ((connect(_sockfd, _dstaddr, _dstaddrlen)) < 0)
	{
		perror("Blad - funkcja connect()\n");
		exit(1);
	}
}

//funkcja akceptuje polaczenie na nasluchujacym gniezdzie
int Accept(int _sockfd, struct sockaddr* restrict _addr, socklen_t* restrict _addrlen)
{
	int _sockfdCli;

	while (1)
	{
		if ((_sockfdCli = accept(_sockfd, _addr, _addrlen)) < 0)
		{
			if (errno == EINTR)
			{
				continue;	//ponow wykonywanie funkcji accept(), jesli zostalo przerwane przez sygnal
			}
			else
			{
				perror("Blad - funkcja accept() \n");
				exit(1);
			}
		}	
		//przerwij petle jesli wykonywanie sie powiodlo
		break;
	}
	return _sockfdCli;
}

//funkcja przesyla bufor
void Write(int _sockfd, const void* _buf, size_t _count)
{
	if ((write(_sockfd, _buf, _count)) < 0)
	{
		perror("Blad - funkcja write()\n");
		exit(1);
	}
}

//funkcja odczytuje tekst z otwartego polaczenia
ssize_t Read(int _fd, void* _buf, size_t _count)
{
	ssize_t _recvsize;

	if ((_recvsize = read(_fd, _buf, _count)) < 0)
	{
		perror("Blad - funkcja read()\n");
		exit(1);
	}
	return _recvsize;
}

//funkcja zamyka aktywne polczenie
void Close(int _sockfd)
{
	if ((close(_sockfd)) < 0)
	{
		perror("Blad - funkcja close()\n");
		exit(1);
	}
}