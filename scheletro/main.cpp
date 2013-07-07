#include <iostream>
#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include "Sensor.h"
#include "SensorReading.h"
#include "SensoreI2C.h"

#define SIMULATION true

void threadComunicazioneServer(); //definito piu' in basso

using namespace std;

//GLOBAL: (la coda dei dati da inviare, condivisa fra i due thread)
list<SensorReading> lettureDaInviare;
std::mutex mutexLettureDaInviare;
std::condition_variable ciSonoLettureDaInviare;

int main(int argc, char* argv[]) {
	Sensor polveri(01,"pm"), temp(02,"Celsius"), umidita(03,"%"), temp_rasp(11,"Celsius"), umidita_rasp(12,"%");
	
	cout << "Crowdsensing v0.0" << endl;

	//TODO: Inizializza USB e I2C

	//Inizializzazione I2C
	SensoreI2C sensoreInterno;
	unsigned int umiditaInterna, temperaturaInterna, cicliDaUltimaRichiesta = 0;

	int error = sensoreInterno.init();
	if (error!=0) 
	{	
		cout << "Errore inizializzazione i2c. Uscita forzata." << endl;
		exit(1);
	}

	//avvio il thread di comunicazione col server
	std::thread thread2(threadComunicazioneServer);

	auto ultimoSalvataggio = std::chrono::system_clock::now();
	while(1) //while dell'USB (ogni 8ms)
	{
		if(SIMULATION) std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
		
		//TODO: leggi tutti i sensori

		cicliDaUltimaRichiesta++;
		if (cicliDaUltimaRichiesta >=4)
		{
			sensoreInterno.humidity_and_temperature_data_fetch(&umiditaInterna,&temperaturaInterna);
			//TODO: sta roba andrebbe fatta nella classe sensoreI2C
			printf("UMIDITA': %d percento\n",umiditaInterna*100/16382);
			printf("TEMP    : %d C\n",temperaturaInterna*165/16382 - 40);
			sensoreInterno.send_measurement_request();
			cicliDaUltimaRichiesta = 0;
		}

		polveri.aggiungiMisura(5);
		temp.aggiungiMisura(5);
		umidita.aggiungiMisura(5);
		temp_rasp.aggiungiMisura(5);
		umidita_rasp.aggiungiMisura(5);

		auto millisecondiDaUltimoSalvataggio = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - ultimoSalvataggio).count();
		if (millisecondiDaUltimoSalvataggio > 1000) //5*60*1000=5 minuti
		{
			ultimoSalvataggio = std::chrono::system_clock::now();
			{
				//ottengo il mutex per scrivere nella coda
				std::lock_guard<std::mutex> l(mutexLettureDaInviare);
				//TODO: forse si potrebbe usare try_lock_for() per evitare di perdere la sincronizzazione usb
				//aggiunge tutti i valori alla coda
				lettureDaInviare.push_front(SensorReading(polveri));
				lettureDaInviare.push_front(SensorReading(temp));
				lettureDaInviare.push_front(SensorReading(umidita));
				lettureDaInviare.push_front(SensorReading(temp_rasp));
				lettureDaInviare.push_front(SensorReading(umidita_rasp));
			}
			
			//notifico l'altro thread che c'e' roba da inviare in coda:
			ciSonoLettureDaInviare.notify_one();

			//resetto i conteggi per media e varianza
			polveri.reset();
			temp.reset();
			umidita.reset();
			temp_rasp.reset();
			umidita_rasp.reset();
		}
	}

	thread2.join();
	return 0;
}


void threadComunicazioneServer()
{
	list<SensorReading> listaLocale;
	while(1)
	{
		std::unique_lock<std::mutex> ul(mutexLettureDaInviare);
		while (lettureDaInviare.empty())
		{
			//attende (senza consumo risorse) che ci siano nuovi dati da inviare
			ciSonoLettureDaInviare.wait(ul);
		}
		//copia tutti i dati da inviare nella coda locale, e svuota lettureDaInviare
		listaLocale.splice(listaLocale.begin(),lettureDaInviare);
		//rilascio il mutex
		ul.unlock();

		if (!listaLocale.empty())
		{
			//TODO: prova a inviare al server tutti i rilevamenti contenuti nella listaLocale
		}
	}
}
