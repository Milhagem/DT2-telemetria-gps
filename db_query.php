<?php

// REPLACE with your Database hostname
$hostname = "HOSTNAME";
// REPLACE with your Database port
$port = 00000;
// REPLACE with your Database name
$dbname = "DB_NAME";
// REPLACE with Database user
$username = "DB_USERNAME";
// REPLACE with Database user password
$password = "DB_PASSOWRD";

// Create connection
$conn = new mysqli($hostname, $username, $password, $dbname, $port);
// Check connection
if ($conn->connect_error) {
    die("Connection failed: " . $conn->connect_error);
} 

// SQL query
$sql = "SELECT lat, lng FROM gps WHERE reading_time > '2023-02-15';";

$result = $conn->query($sql);

while ($data = $result->fetch_assoc()){
    $gps_data[] = $data;
}

$lats = array_column($gps_data, 'lat');
$longs = array_column($gps_data, 'lng');
$potencias = array_column($gps_data, 'potencia');
$readings_time = array_column($gps_data, 'reading_time');

for($i=0; $i<count($gps_data); $i++) {
    $arrLatLng[0][] = "{location: new google.maps.LatLng(".$lats[$i].",".$longs[$i]."), weight: 5"."}";
}

// Convert to JSON
$coordenadas = json_encode($arrLatLng);

// $resultado treatment
$coordenadas = str_replace("[[","[", $coordenadas);
$coordenadas = str_replace("]]","]", $coordenadas);
$coordenadas = str_replace('"','', $coordenadas);

$conn->close();

?>