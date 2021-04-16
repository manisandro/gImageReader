# How to translate docs (Help files)

## 1. Install:

https://github.com/mkdocs/mkdocs/

https://squidfunk.github.io/mkdocs-material/

https://pypi.org/project/mkdocs-static-i18n/

https://pypi.org/project/mkdocs-redirects/

## 2. Copy:

File: `mkdocs.yml`

and

Folder: `/docs`

in to the same location *(\*don't put mkdocs.yml in /doc folder!)*

## 3. Edit mkdocs.yml

Add your language under

`languages:`

`alternate:`

`redirect_maps:`

## 3. Translate:

In Folder `/docs` duplicate, rename and translate every `*.\<lang>.md` file

example for German: Clone `index.md` -> `index.de.md` 

## 4. Test

in your console go to the folder of mkdocs.yml then run

`mkdocs server`

Open your web-browser and go to http://127.0.0.1:8000

## 5. Compile 

`mkdocs compile`

compiled site is in folder `\site` that is in same folder where the  `mydocs.yml` is