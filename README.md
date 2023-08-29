# Progetto PMCSN - Progettazione, simulazione e valutazione delle prestazioni di un'architettura a microservizi
Il caso di studio simula un'architettura a microservizi per identificare il numero ottimale di serventi necessari per soddisfare determinati QoS e contemporaneamente minizzare il costo totale (inteso come numero di serventi aggiunti).

- La directory ```source``` contiene il programma per eseguire la simulazione sul sistema per tutte le modalità (FINITE|INFINITE) e topologie (BASE|RESIZED|IMPROVED).
- La directory ```doc``` contiene la documentazione associata al caso di studio in esame.
- La directory ```analysis``` contiene i risultati prodotti dalle simulazioni per l'analisi transiente e a steady-state, sia nel formato csv che nel formato xlsx (e il confronto tra le diverse configurazioni).

## Esecuzione
- Spostarsi sulla directory principale del progetto
- Eseguire il programma tramite il seguente script:
    ```bash
    ./run_simulation.sh -m MODE -t TOPOLOGY
      -m MODE: modalità di simulazione [FINITE|INFINITE]
      -t TOPOLOGY: topologia del sistema [BASE|RESIZED|IMPROVED]
    ```
- Lo script si occupa di creare le directory ```bin``` e ```analysis```, che conterranno rispettivamente l'eseguibile prodotto tramite il Makefile e i risultati generati dalla precisa simulazione scelta da eseguire.
