suneido -dump stdlib
suneido -dump suneidoc
suneido -dump imagebook
copy z:\software\suneido\translatelanguage.su
call saveaway suneido.db
suneido -load stdlib
suneido -load suneidoc
suneido -load imagebook
suneido -load translatelanguage.su
copy z:\software\suneido\beta\jsuneido.jar
del suneido%1.zip
zip -9 suneido%1.zip suneido.exe suneido.db scilexer.dll libhpdf.dll clucene.dll hunspell.exe en_US.aff en_US.dic splash.bmp jsuneido.jar
del jsuneido.jar
du -h suneido%1.zip
del suneido.db
unsave suneido.db
