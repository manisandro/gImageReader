# Možnosti programa

- Možnosti programa ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/categories/preferences-system.png) najdete v meniju skrajno desno zgoraj ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/categories/preferences-system.png). Če poganjate program v grafičnem okolju Gnome 3 desktop je ta meni del namizne vrstice goraj.

- Možnosti omogočajo nastavitev izhodnih pisav, kakor tudi če naj aplikacija opomni o mankajočih slovarjev črkovalnikov ter novih verzijah pograma.

- V kolikor poganjate program v grafičnem okolju Gtk+ vam tudi omogoča nastavitev orientacije vrstice rezultata

- *Mesta jezikovnih podatkov* določa ali so jezikovne defnicije in slovaji shranjeni na globalni sistemski lokacji (npr.: `%ProgramFiles%` za Windows oz za Linux ponavadi v `/usr` ) ali pa le v uporabniškem območju (torej znotraj uporabnikovega direktorija). To je porabno v kolikor uporabnik nima globalnih pravic pisanja.

- Dodatno lahko tam definirate nova pravila a ujemanje z jezikovnimi definicijami tesseract-a (nažalost jezikovne definicije za tesseract ne vsebujejo teh informacij). Seznam pred definiranih pravil lahko vidite na prvem seznamu. Pri dodajanju jezikovnih pravil je potrebno dodati 3 polja in sicer:

***Predpona imena datoteke***: Ime jezikovne datoteke za tesseract je vedno oblike *<predpona>.traineddata*, npr. za Angleščino, je ime datoteke *eng.traineddata* torej je predpona *eng*.

***ISO Koda***: To je [ISO 639-1 jezikovna koda](http://en.wikipedia.org/wiki/List_of_ISO_639-1_codes) (npr. *en*), opcijsko kombinirana z podčrtajem [ISO 3166-2 regijske kode](http://en.wikipedia.org/wiki/ISO_3166-2) (npr. *en_US*). Ta podatek je potreben za ujemanje slovarjev črkovalnikov na prepoznan jezik. Ta pododatek ni kritično potreben, a je potreben za samodejno nameščanje slovarjev črkovalnkov. Ta šifra se lahko tudi izmisi, če ne obstaja primerna izbira. Posledica bo le ne bo konrole ali je beseda najdena tudi v slovarju.

***Izvorno ime***: Ime jezika, ki bo prikazano v meniju možnih pisav.