# Uporaba

## Odpiranje in uvažanje slik

- Slike se odprejo/uvozijo pod območjem *Viri*, ki se prikaže/skrije ob kliku na gub ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/insert-image.png) skrajno levo v *glavni vrstici*.
- Da odprete obstoječo Sliko ali PDF dokument, klknite na gum *Dodaj slike* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/document-open.png) na vrhu območja *Viri*. 
- Da odprete VSE Slike ali PDF-e v nekem direktorju, kliknite na gump *Dodaj mapo* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/qt/data/extra-theme-icons/folder-open.png).
- Zajetje slike iz zaslona naredite s klikom na *Posnemi zaslon* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/devices/camera-photo.png).
- Sliko lahko tudi vstavite iz doložišča s klikom na *Prilepi* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/edit-paste.png).
- Če želite odpreti nedavno že odprto datoteko, kliknite na puščico![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/pan-down-symbolic.symbolic.png)zraven *Dodaj slike* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/document-open.png).
- Sezam odprtih slik pravljate s gumbi  ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/list-remove.png)   ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/places/user-trash.png)   ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/edit-clear.png). 
  Začasne datoteke (kot so posnetki zaslona, slike iz odložišča) se samodejno pobrišejo, ko se program zapre.
- Za zajem slike iz optičnega čitalca(Skenerja), kliknite na zavihek *Pridobi* na območji *Viri* ter nato na gumb *Zaznaj* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/devices/scanner.png).

## Pogled in prilagoditev slik

- Da sliko približate ali odaljite uporabite gumbe v *glavni vstici*  ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/zoom-in.png)![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/zoom-out.png)![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/zoom-original.png) 
   namesto gumbom lahko tudi uporabite CTRL + kolešček na miški
- Za obračanje slike za 90 stopinj uporabite![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/object-rotate-left.png)![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/object-rotate-right.png) ter ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/rotate_page.svg)za fino rotacijo slike.
- Osnovno obdelavo slike izvedete z orodji v *kontrolni vrstici za slike* ,ki se prikaže/skrije z klikom na kumb *Kotronilniki slike* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/controls.png)  ki se nahaja v *glavni vrstici*. Orodja, ki so tam na voljo so prilagoditev *svetost*![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/brightness.png) ter *kontrast* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/contrast.png) kakor tudi resolucija slike ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/resolution.png) (uporablja algoritem interpolacije).
- Lahko se označi več slik, kar omogoča prilagoditev več slik na enkrat.

## Priprava na prepoznavo OCR

- Za izvedbo OCR (Ang: *Optical Character Recognition*) na sliki, mora uporabnik definirati:        

  - Vhodne slike ( slike z besedilom, ki ga želimo prepoznati),
  - Način prepoznave (*Neoblikovano besedilo* / *hOCR ali PDF*)
  - Jezik(e) prepoznave ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/qt/data/extra-theme-icons/applications-education-language.png).
- **Vhodne slike**  so izbrani vnosi v zavihku *Datoteke* , ki se nahaja v območju *Viri*. Če je izbranih več slik, bo program smatral nabor slik, kot večstranski dokument, ter ob zgonu prepoznave pozval uporabnika katere strani želi, da jih prepozna .
- **Način prepozave**(*Način OCR*) se izbere v spustnem seznamu *Način OCR* , ki se nahaja v *glavni vrstici*:
- ***Neoblikovano besedilo*** OCR pogonu, bo izluščil le golo besedilo, brez kakršnega koli zaznavanja oblikovanja ali postavitve.
  - ***hOCR ali PDF*** OCR pogon bo izluščil besedilo kot  *hOCR* html dokument, ki vsebuje oblikovanje ter postavitev za prepoznano stran. *hOCR* je standardni zapis za hrambo rezultatov prepoznave in se lahko uprablja za dodatno obdelavo s programi, ki poznajo ta standard.  gImageReader lahko nadaljno obdeluje *hOCR* datoteke in tako iz tega ustvari PDF ali ODT.
- **Jezik prepoznave** se izbere v spustnem seznamu ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/qt/data/extra-theme-icons/applications-education-language.png) ,ki je desno od of the od gumba ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/insert-text.png) *Prepoznaj* v *glavni vrstici*. Če je slovar prepoznave nameščen za jezikovno definicijo tesseract, je možno tudi izbirat med regijskimi slovarji tega jezika. To le vpliva na končno opozarjanje v kolikor zaznana beseda ni v slovarju. *Ne prepoznane jezikovne definicije tesseract podo prikazane le z njihovo datotečno predpono a možno je v program vnesti ustrezna pravila prikaza - glej RTFM*.
- **Več jezikov prepoznave** se lahko izbere na enrkat prav tako v spustnem seznamu ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/qt/data/extra-theme-icons/applications-education-language.png) . Dodatni jeziki se lahko nameščajo pod *Upravljaj z jeziki...* , ki ga najdete tudi v ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/qt/data/extra-theme-icons/applications-education-language.png)  Več o tem v poglavju ***Tesseract jezikovne definicije***.

## Prepoznava in nadaljna obdelava - Neoblikovano besedilo

- Območja, ki se jh naj prepozna se znači z miško (levi klik + premik miške)  kot pravokotno območje okrog območja slike.
- Več območij se označi tako, da držimo tipko CTRL med tem ko izbiramo.
- Namesto ročnega izbora se lahko tudi uporabi, gumb *Samodejno prepoznaj razporeditev*![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/autolayout.png) , ki je v *glavni vrstici*. Izvršeno bo poizkus samodejne repoznave območjih, kakor tudi samodejna rotacija slike v kolikor je le to potrebno.
- Območja se lahko izbrišejo ter spremeni zaporedje z menijem, ki se pojavi, ko na območje kliknemo z desnim miškinim gumbom. Dimenzijo območja lahko spremenimo z klikom na rob območja.
- Izbrana območja slike (ali cela slika, v koikor ni definiranih nobenih območij) se prepozna z klikom na gumb *Prepoznaj izbor* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/insert-text.png) v *glavni vrstici*.
- Če želimo prepoznavo samo enega od večih območji, kliknemo desno na želenem območju. Iz istega menija, je možno tudi prepoznano besedilo preusmeriti na odložišče namesto na območje *rezulat*.
- Če je izbranih več strani za prepoznavo, program nudi prepoznavo istih območjih na vseh straneh ali samodejno prepozavo na vsaki strani posebej.
- Preponano besedilo se bo prikazalo v območju *Rezultat* (razen če je bilo besedilo preusmerjeno v odožišče. Prikazuje se samodejno in sproti, kakor hitro je bil del besedila prepoznan.
- V kolikor je na voljo slovar za prepoznano besedilo, bo vklopljen samodejni črkovalnik na izhodem besedilo. Uporabljan slovar črkovalnika lahko spremenimo pod ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/qt/data/extra-theme-icons/applications-education-language.png) zraven gumba *Prepoznaj območje* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/insert-text.png), ali iz menija, ki se pojavi če kliknemo z desnim miškinim gumbom na območju *Rezultat*.
- Ko bo prepoznano dodatno besedilo, bo to odvisno od nastavitev ali pripteto na dnu![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/ins_append.png), ali na pozici kazalke![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/ins_cursor.png) ali zamenjalo celotno prešnjo vsebino![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/ins_replace.png). Le to nastavitev izberete v meniju *Izberi način vstavljanja*, ki ga najete skrajno levo v območju *Rezultat*.
- Drugi načini nadaljne obdelave so med drugimi, odstranitev prelomov vrstic, združevanje dolgih presledkov ter drugo. Kakor tudi iskanje ter zamenjava besed. Najdmo jih v meniju, ki se odpre ob kliku na puščico ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/gtk_skel/share/icons/hicolor/16x16/actions/pan-down-symbolic.symbolic.png)zraven![](https://raw.githubusercontent.com/manisandro/gImageReader/master/data/icons/stripcrlf.png)
- Možno je tudi nastaviti seznam samodejnih zamenjav besed pod *najdi in zamenjaj* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/edit-find-replace.png)ter nato z klikom na gumb *Zamenjave* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/document-edit.png).
- Spremembe v besedilu v območju *Rezultat* se lahko razveljavijo ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/edit-undo.png) ter ponovno uveljavijo ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/edit-redo.png) z klikom na ustrezni gumb v vrstici območja *Rezultat*.
- Vsebina besedila v območju *Rezultat* lahko shranite v datoteko z klikom na gumb *shrani* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/document-save-as.png) v vrstici območja *Rezultat*.

## Prepoznava in nadaljna obdelava - hOCR ali PDF

- V načinu *hOCR ali PDF*, se vedno prepoznava celotna stran izbranega vira oz virov.

- Rezultat prepoznave je predstavljen v območju *Rezultat* v obliki drevesne strukture, razdeljen v strani, odstavke, vrtice, besede, grafiko.

- Ko kliknemo na posamezni vnos v drevesni stukturi, se obarva pripadajoče območje na sliki.  Posamezni parametri postavitve, oblikovanja.. se prikazujejo sočasno v zavihku *Lasnosti*. Surova hOCR koda je vidna v zavihku *Koda*.

- Beseda v dokumetni drevesni strukturi se lahko popravi z dvoklikom na dotično besedo. Če je beseda ni slovnična(ni v slovarju) bo obarvana rdečo. Desni klik na dotično besedo vam ponudi meni s predlogi za popravek.

- Vrednosti za izbrano entiteto se lahko popravijo z klikom na vrednost v zaviku *Lasnosti*. Zanimive vrednosti za popravke pri besedilih so izbojšava vsebinskega območja(bbox), sprememba jezika ter tipa in velikosti pisave. Vsebinsko območje se lahko tudi popravi tako, da z miško premaknemo rob pravokotnega območja na delovni površini.

- Razbite besede se lahko združijo, tako da jih označimo(ctrl+miškin klik) ter nato z desnim klikom nad besedo izberemo *zdriži*.

- Odvečne elemente odstranimo z desnim klikom na element v drevesni strukturi ter odstrani..

- Dodatna območja grafike se definarijo tako, da v drevesni strukturi na veji tiste strani kliknemo z desno miškini tipko izberemo *Dodaj področje grafike* ter na delovni površini narišemo ustrezen pravokotnik.

## Izvoz rezultata

### hOCR

- Drevesno strukturo dokumenta lahko shranite kot  *hOCR HTML dokument* s klikom na *Shrani kot besedilo hOCR* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/document-save-as.png) v vrstici območja *Rezultat* 

- Obstoječi *hOCR HTML* dokument se lahko uvozi z gumbom *Odpri dokument hOCR* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/document-open.png)  v vrstici območja *Rezultat*.

### PDF

- **PDF** datoteka se lahko ustvari preko menija *Izvozi* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/document-export.png) ter nato *Izvozi kot PDF* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/mimetypes/application-pdf.png) v vrstici območja *Rezultat*. 

- Za izvoz v PDF sta na voljo dva zapisa:

1. ***PDF*** bo ustvaril PDF z enako razporeditivijo in grafikami/slikami kot je bil zanan v izvorni sliki.
2.  ***PDF z nevidno plastjo besedilo*** bo ustvaril PDF z nespremenjeno sliko vira kot ozadje ter čez nevidena plast besedila, ki pa se ga lahko označi. To je koristno za ustvarjanje dokumentov, ki so vizualno identični vhodnemu a imajo možnost iskanja ter kopiranja besedila.

- Pri izvozu v PDF, se uporabnik tudi lahko odloči ali bo določil tip i velikost pisave ali naj ostane samodejno zaznana. Prav tako lahko določi barvno globino, kompresijo, resolucijo za slike v izhodnem PDF dokumentu, da tako optimizira velikost datoteke.

### ODT

- **ODT** datoteka se lahko ustvari preko menija *Izvozi* ![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/actions/document-export.png) ter nato *Izvozi kot ODT*![](https://raw.githubusercontent.com/manisandro/gImageReader/master/packaging/win32/skel/share/icons/hicolor/22x22/mimetypes/x-office-document.png) v vrstici območja *Rezultat*. 