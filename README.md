# Procesul de dirijare a pachetelor intr-un router - Tema 1 PC

Student: Andrei Pantelimon
Group: 325 CA

Am implementat functionalitatea de dirijare a pachetelor intr-un router, inclusiv protocolul ARP, protocolul ICMP si modificarea checksum-ului folosing algoritmul incremental din RFC 1624.

# Tabela de rutare
Am inceput prin a parsa tabela de rutare, citind cate o linie si prelucrand-o pentru a obtine prefixul, next-hop-up, masca si interfata si introducandu-le intr-o structura. Apoi am sortat tabela crescator dupa prefix, iar in cazul entry-urilor cu prefixe egale, descrescator dupa masca. Pentru a cauta in tabela de rutare folosesc o cautare binara, ce nu se opreste la primul element gasit, ci merge mai departe pana gaseste primul element ca index de la stanga la dreapta in vectorul sortat (first occurence).

# Procesul de dirijare

Ca structura de baza am folosit, pe langa scheletul de cod oferit, implementarea laboratorului 4, deoarece functionalitatea este aceeasi. Am inceput prin a verifica daca un pachet primit de router este de tip IP sau de tip ARP. Pentru procesul de dirijare ne intereseaza pachetele IP. Dupa ce am stabilit pointeri pentru headerele de Ethernet, de IP si de ICMP, am inceput verificarea. Am "aruncat" orice pachet care:
1. Nu avea TTL > 1
2. Nu avea checksum-ul corect
3. Ruta pe care trebuia trimis pachetul era inexistenta in tabela de rutare.

## Protocolul ICMP

Am implementat protocolul ICMP astfel:
1. Pentru cazul de TIMEOUT si de HOST_UNREACHABLE, se apeleaza functia send_ICMP_reply din main ce creeaza un pachet de la 0, ii completeaza cele 3 headere si il trimite ca reply
2. Pentru cazul in care routerului ii este cerut un ECHO Reply, completez in continuare headerul de IP si de ICMP si trimit acelasi pachet inapoi.

## Protocolul ARP

Protocolul ARP este folosit atunci cand cunoastem adresa IP a unei statii dar nu ii cunoastem adresa MAC. Am implementat 3 functionalitati:
1. Atunci cand nu cunoastem adresa MAC, routerul pune pachetul curent intr-o coada si apoi trimite un request in Broadcast pentru a afla adresa acestuia, creeand un pachet de tip ARP de la 0 si trimitandu-l.
2. Cand routerul primeste un pachet de tip ARP Request va trebui sa trimita un pachet de tip ARP Reply cu adresa mac a acestuia, prin aceeasi metoda de a crea un pachet de la 0.
3. Cand acesta primeste un pachet de tip ARP Reply, el va redimensiona tabela arp daca este nevoie, va adauga un entry in tabela si va goli coada, trimitand pachetele ramase.

## Probleme & Bug-uri

Pana acum, stiu de existenta a doua probleme netestate in checker pe care nu am reusit sa le rezolv:
1. Rezolvarea conflictului intre pachete. Spre exemplu, host-ul A vrea sa trimita un pachet hostului B. Nici unul din ele nu au adresa MAC a celuilalt. A trimite request, pana sa primeasca reply, acesta continua sa trimita pachete (in cazul unui ping) ce intra in stiva. B trimite arp request si el si incepe sa trimita reply (de ping de exemplu), pachete ce intra in aceeasi stiva. De aceea, in implementare exista un mic packet loss.
2. In wireshark se pot observa pachete duplicate; dar aceasta SE POATE datora si metodei de testare a checkerului.

## Bonus

Am implementat bonusul folosind documentatia din RFC 1624. 
> Given the following notation:
HC - old checksum in header
C - one's complement sum of old header
HC' - new checksum in header
C' - one's complement sum of new header
m - old value of a 16-bit field
m' - new value of a 16-bit field

>RFC 1071 states that C' is:
C' = C + (-m) + m' -- [Eqn. 1]
= C + (m' - m)

>As RFC 1141 points out, the equation above is not useful for direct use in incremental updates since C and C' do not refer to the actual checksum stored in the header. In addition, it is pointed out that RFC 1071 did not specify that all arithmetic must be performed using one's complement arithmetic.

>Finally, complementing the above equation to get the actual checksum, RFC 1141 presents the following:
HC' = ~(C + (-m) + m')
= HC + (m - m')
= HC + m + ~m' -- [Eqn. 2]

Folosind ecuatia nr.2: HC' = HC + (m - m').
Dar, cerinta a fost de a implementa pentru decrementarea TTL. Atunci ecuatia (m - m') se rezuma in a aduna 1.
Rezultatul consta in a incrementa checksum-ul cu 1, in cazul decrementarii TTL cu 1.


