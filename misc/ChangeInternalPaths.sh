#!/bin/bash
# This is a horrible hack for a bug in MacOSX that hard-codes paths in libs

# Run this only from a terminal line on a brand new Gnaural.app bundle you just created on your Desktop. 
#It un-links all the libs. To check that it worked, try running "otool -L *" from in the libs dir and 
#also on the Gnaural-bin executable.
#Be sure these paths are right for your system (the Gnaural.app bundle is usually created on the Desktop,
#and the local libs are in ~/gtk/inst/lib):
#LIBDEFAULTDIR=~/gtk/inst/lib
ORIGLIBDEFAULTDIR=~/gtk/inst/lib
LIBDEFAULTDIR=~/Desktop/Gnaural.app/Contents/Resources/lib
SODEFAULTDIR=~/Desktop/Gnaural.app/Contents/Resources/lib/gtk-2.0/2.10.0/loaders
EXEDEFAULTDIR=~/Desktop/Gnaural.app/Contents/MacOS

#go in to libs directory:
cd $LIBDEFAULTDIR

#first reset each lib ID:
for file in *.dylib
 do
  echo "Resetting internal ID for lib" $file "to @executable_path/../Resources/lib/"$file
# this is what you do for IDs:
  install_name_tool -id @executable_path/../Resources/lib/$file $file
 done

#now reset each path to contingencies (yuck!!!):
for file2 in *.dylib
 do
  for file1 in *.dylib
   do
    echo "Resetting internal path for lib" $file2 "from" $ORIGLIBDEFAULTDIR/$file1 "to @executable_path/../Resources/lib/"$file1
  # this is what you do for executables:
    install_name_tool -change $ORIGLIBDEFAULTDIR/$file1 @executable_path/../Resources/lib/$file1 $file2 
   done
 done

echo "NOW Resetting internal paths for for *.so in directory" $SODEFAULTDIR
#Now for "*.so" so reset each path to contingencies (yuck!!!):
#stay in lib directory!:
cd $LIBDEFAULTDIR
for file2 in $SODEFAULTDIR/*.so
 do
  for file1 in *.dylib
   do
    echo "Resetting internal path for lib" $file2 "from" $ORIGLIBDEFAULTDIR/$file1 "to @executable_path/../Resources/lib/"$file1
  # this is what you do for executables:
    install_name_tool -change $ORIGLIBDEFAULTDIR/$file1 @executable_path/../Resources/lib/$file1 $file2 
   done
 done


#go in to libs directory again:
cd $LIBDEFAULTDIR

#finally, fix the gnaural executable:
  for file1 in *.dylib
   do
    echo "Resetting internal paths in gnaural from" $ORIGLIBDEFAULTDIR/$file1 "to @executable_path/../Resources/lib/"$file1
  # this is what you do for executables:
    install_name_tool -change $ORIGLIBDEFAULTDIR/$file1 @executable_path/../Resources/lib/$file1 $EXEDEFAULTDIR/Gnaural-bin
   done

