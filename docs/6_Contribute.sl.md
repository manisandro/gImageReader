# Prispevaj svoje znanje/izkušnje
## Prevajanje
Za predloge ali prispevke katere koli vrste, prosimo ustvarite *ticket* in ali *pull-request* na [GitHub strani projekta](https://github.com/manisandro/gImageReader), ali me kontaktirajte na [manisandro@gmail.com](mailto:manisandro@gmail.com). Še posebej sem vesel prevodov - Tukaj so glavni koraki za ustvarjanje prevoda:

A) gImageReader prevodi se nahajajo na [weblate](https://hosted.weblate.org/projects/gimagereader/) od koder se periodično samodejno prenesejo.

ali

B):

1. Prenesite [zadnjo izvorno kodo](https://github.com/manisandro/gImageReader/archive/master.zip).
2. Odprite mapo `po` .
3. Da naredite novi prevod skopirajte `gimagereader.pot` datoteko v `<language>.po` (npr. `de.po` za Nemščino). Za popravke obstoječega prevoda pa izberite že obstoječi.
4. Prevedite nize v `<language>.po`.
5. Pošljite datoteko **po** na [manisandro@gmail.com](mailto:manisandro@gmail.com). Hvala!

## Programski hrošči in podpora

Če najdete težavo ali imate predlog ustvarite *ticket* na [gImageReader issue tracker](https://github.com/manisandro/gImageReader/issues), ali me kontaktirajte direktno na [manisandro@gmail.com](mailto:manisandro@gmail.com). Preverite tudi obstoječe [FAQ](https://github.com/manisandro/gImageReader/wiki/FAQ). Če se vam dogajajo "zamrznitve" ali "sesuja" pograma poizkusite v ticket/mail podat sledeče informacije:

- Če je pojavi poročilo o sesutju, priložite naveden sklad, ki je viden tami. Da ste prepričani, da je sklad popolen in uporabljate Linux preverite da imaste nameščen razhroščevalnik `gdb` kakor tudi  simboli razhroščevalnika v kolikor jih vaša distribucija nudi. Paketi, ki le to vsebujejo se ponavai imenujejo **<packagename>-debuginfo** ali **<packagename>-dbg**. V kolikor uporabljate Windows, so nekateri ustrezni simboli že privtzeto nameščeni.
- Če uporabljte Windows, priložite še log datoteki `%ProgramFiles%\gImageReader\gimagereader.log` ter`%ProgramFiles%\gImageReader\twain.log` .
- Če uporabljate Linux, zaženite aplikacijo v terminalu ter priložite vse kar se vam izpiše v terminalu.
- Poizkusite opisat, kar se da natančno kaj počnete in v koliko se da težavo reproducirat.