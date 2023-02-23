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

// Keep this API Key value to be compatible with the ESP32 code.
$api_key_value = "API_KEY_VALUE";

$api_key = $lat = $lng = $reading_time = "";

if ($_SERVER["REQUEST_METHOD"] == "POST") {
    $api_key = test_input($_POST["api_key"]);
    if($api_key == $api_key_value) {
        $lat = test_input($_POST["lat"]);
        $lng = test_input($_POST["lng"]);
        
        // Create connection
        $conn = new mysqli($servername, $username, $password, $dbname);
        // Check connection
        if ($conn->connect_error) {
            die("Connection failed: " . $conn->connect_error);
        } 
        
        $sql = "INSERT INTO gps (lat, lng)
        VALUES ('" . $lat . "', '" . $lng . "')";
        
        if ($conn->query($sql) === TRUE) {
            echo "New record created successfully";
        } 
        else {
            echo "Error: " . $sql . "<br>" . $conn->error;
        }
    
        $conn->close();
    }
    else {
        echo "Wrong API Key provided.";
    }

}
else {
    echo "No data posted with HTTP POST.";
}

function test_input($data) {
    $data = trim($data);
    $data = stripslashes($data);
    $data = htmlspecialchars($data);
    return $data;
}