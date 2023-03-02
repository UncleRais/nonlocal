#!/bin/bash
#sed -i 's/\r$//' script.bash
line="------------------------------------------------------"
echo "Hello! You are working with thermal conductivity equation solver."
echo $line


echo "Searching for build directory..."
build_dir=../build/
if [ -d $build_dir ]; then
	echo "The $build_dir directory exists."
	cd $build_dir
else
	echo "The $build_dir directory does not exist"
	ls
fi
echo $line


echo "Starting make thermal_nonstationary_tests_1d... "
time make thermal_nonstationary_tests_1d 
echo $line


main_dir=build/calculations
way_to_folder="D:/nonlocal/"$main_dir
function calculation {
file_name="$1"
echo $file_name 
mkdir $way_to_folder/$2/$file_name  
time ./src/1d/thermal/thermal_nonstationary_tests_1d $way_to_folder/$2/$file_name 
}
echo "Starting calculations..."
echo "All results will be written in folder $way_to_folder ."
echo $line

#------------------------------------------------------------------------------------------------------------------------
# КАК ПРОВОДИТЬ ЕДИНИЧНЫЙ РАСЧЕТ:
# 1) создание папки для сохранения данных : mkdir $way_to_folder/<название_папки>
# 2) calculation <название расчета> <папка расчета>
#------------------------------------------------------------------------------------------------------------------------

mkdir $way_to_folder/"tests"
calculation  "convetion_1d" "tests"