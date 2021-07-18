# תרומה

לקבלת הצעות ו / או תרומות מכל סוג שהוא אנא הגישו בקשות-השפעה באתר [דף הפרוייקט של GitHub](https://github.com/manisandro/gImageReader), או צרו איתי קשר בכתובת [manisandro@gmail.com](mailto:manisandro@gmail.com). אודה במיוחד לתרגומים - להלן השלבים העיקריים ליצירת תרגום:

א): תרגומי gImageReader מתארחים ב- [weblate](https://hosted.weblate.org/projects/gimagereader/) וממוזגים מעת לעת ל- master.

או

ב):

1. הורד את [ארכיון המקור האחרון](https://github.com/manisandro/gImageReader/archive/master.zip).
2. היכנס לתיקייה `po`.
3. כדי ליצור תרגום חדש, העתק את הקובץ `gimagereader.pot` ל- `<language>.po` (כלומר `de.po` עבור גרמנית). כדי לערוך תרגום קיים, פשוט בחר את הקובץ המתאים.
4. תרגם את המחרוזות ב- `<language>.po`.
5. שלח את הקובץ **po** אל [manisandro@gmail.com](mailto:manisandro@gmail.com). תודה!

## ניפוי באגים ותמיכה

אם אתה מוצא בעיה כלשהי או שיש לך הצעה, אנא הגש דו"ח אל [מעקב הבעיות של gImageReader](https://github.com/manisandro/gImageReader/issues), או צור איתי קשר ישיר בכתובת[manisandro@gmail.com](mailto:manisandro@gmail.com). הקפד להתייעץ גם ב- [שאלות נפוצות](https://github.com/manisandro/gImageReader/wiki/FAQ). אם אתה חווה קריסות או נתק, אנא נסה גם לכלול את המידע הבא בדו"ח / אימייל:

- אם מופיע מטפל הקריסות, כלול את ה- backtrace המוצג שם. בכדי לוודא שה- backtrace הושלם, אם אתה מריץ את התוכנית ב- Linux, ודא שמנפה הבאגים `gdb`,  כמו גם סמלי ניפוי הבאגים, הם מותקנים אם ההפצה שלך מספקת אותם. בדרך כלל החבילה המכילה את סמלי הבאגים נקראת **<packagename>-debuginfo** או **<packagename>-dbg**.
- אם אתה מריץ את התוכנית ב- Windows, כמה סמלים של מנפה הבאגים כבר מותקנים כברירת מחדל. וקבצי היומן הם `%ProgramFiles%\gImageReader\twain.log`.
- אם אתה מריץ את התוכנית ב- Linux, הרץ שוב את התוכנית דרך Terminal וכלול את כל הפלט המופיע ב- Terminal.
- נסה לתאר כמה שיותר טוב מה עשית ומה הבעיה הניתנת לשחזור.
