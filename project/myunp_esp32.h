//Funkcje opakowujace funkcji sieciowych - ESP32
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/netif.h"

#ifndef MAXLINE
#define MAXLINE 1024
#endif // !MAXLINE

//na podstawie unp.h R.Stevens

//funkcja tworzaca gniazdo ze sprwdzeniem bledu
int Socket(int _family, int _type, int _protocol)
{
	int _sockfd;
	if ((_sockfd = socket(_family, _type, _protocol)) < 0)
	{
		ESP_LOGI("TCP_SERVER_ERROR","ERROR - socket()");
        vTaskDelete(NULL);
	}
	return _sockfd;
}

//funkcja przydzielajaca strukture adresowa do gniazda
void Bind(int _sockfd, const struct sockaddr* _addr, socklen_t _addrlen)
{
	if ((bind(_sockfd, _addr, _addrlen)) < 0)
	{
		ESP_LOGI("TCP_SERVER_ERROR","ERROR - bind()");
        close(_sockfd);
        vTaskDelete(NULL);
	}
}

//funkcja rozpoczyna nasluchiwanie na gniezdzie
void Listen(int _sockfd, int _backlog)
{
	if ((listen(_sockfd, _backlog)) < 0)
	{
        ESP_LOGI("TCP_SERVER_ERROR","ERROR - listen()");
        vTaskDelete(NULL);
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
				ESP_LOGI("TCP_SERVER_ERROR","ERROR - accept()");
                close(_sockfd);
                vTaskDelete(NULL);
			}
		}	
		//przerwij petle jesli wykonywanie sie powiodlo
		break;
	}
	return _sockfdCli;
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
				ESP_LOGI("TCP_SERVER_ERROR","ERROR - read()");
                vTaskDelete(NULL);
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
				ESP_LOGI("TCP_SERVER_ERROR","ERROR - write()");
				close(_fd);
                vTaskDelete(NULL);
			}
		}

		_n_left -= (size_t) _n_written;		//zaktualizowanie pozostalych do przeslania bajtow
		_dstbuff += _n_written;		//przesuniecie wskaznika na kolejny element bufora
	}
	return _n_written;
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
				ESP_LOGI("TCP_SERVER_ERROR","Blad - funkcja my_read()");
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
			ESP_LOGI("TCP_SERVER_ERROR","Blad - funkcja Readline()\n\r");
			close(_fd);
			vTaskDelete(NULL);
		}
	}

	*_ptr = '\0';		//dodaj na koncu koniec lini
	return(_n);			//zwroc liczbe odczytanyc znakow (bez \0)
}