<?php
require "db_query.php";
?>

<!DOCTYPE html>
  <html lang="pt">
    <head>
      <meta charset="utf-8">
      <link rel="stylesheet" href="styles.css">
      <title>Mapa de Calor</title>
    </head>

    <body>
      <div id="floating-panel">
        <button id="toggle-heatmap">Toggle Heatmap</button>
        <button id="change-gradient">Change gradient</button>
        <button id="change-radius">Change radius</button>
        <button id="change-opacity">Change opacity</button>
      </div>
      <div id="map"></div>

      <script>
        // This example requires the Visualization library. Include the libraries=visualization
        // parameter when you first load the API. For example:
        // <script src="https://maps.googleapis.com/maps/api/js?key=YOUR_API_KEY&libraries=visualization">
        let map, heatmap;

        function initMap() {
          map = new google.maps.Map(document.getElementById("map"), {
            zoom: 17,
            center: { lat: -19.870, lng: -43.960},
            mapTypeId: "satellite",
          });

          heatmap = new google.maps.visualization.HeatmapLayer({
            data: getPoints(),
            map: map,
          });

          document
            .getElementById("toggle-heatmap")
            .addEventListener("click", toggleHeatmap);
          document
            .getElementById("change-gradient")
            .addEventListener("click", changeGradient);
          document
            .getElementById("change-opacity")
            .addEventListener("click", changeOpacity);
          document
            .getElementById("change-radius")
            .addEventListener("click", changeRadius);
        }

        function toggleHeatmap() {
          heatmap.setMap(heatmap.getMap() ? null : map);
        }

        function changeGradient() {
          const gradient = [
            "rgba(0, 255, 255, 0)",
            "rgba(0, 255, 255, 1)",
            "rgba(0, 191, 255, 1)",
            "rgba(0, 127, 255, 1)",
            "rgba(0, 63, 255, 1)",
            "rgba(0, 0, 255, 1)",
            "rgba(0, 0, 223, 1)",
            "rgba(0, 0, 191, 1)",
            "rgba(0, 0, 159, 1)",
            "rgba(0, 0, 127, 1)",
            "rgba(63, 0, 91, 1)",
            "rgba(127, 0, 63, 1)",
            "rgba(191, 0, 31, 1)",
            "rgba(255, 0, 0, 1)",
          ];

          heatmap.set("gradient", heatmap.get("gradient") ? null : gradient);
        }

        function changeRadius() {
          heatmap.set("radius", heatmap.get("radius") ? null : 20);
        }

        function changeOpacity() {
          heatmap.set("opacity", heatmap.get("opacity") ? null : 3);
        }

        // Heatmap data: 500 Points
        function getPoints() {
          return <?php echo $coordenadas; ?> ;
        }

        window.initMap = initMap;
      </script>
      <script 
      src="https://maps.googleapis.com/maps/api/js?key=AIzaSyAXUTGwlqmbnNMxEgI4zzsPVzeksIZmDMU&callback=initMap&libraries=visualization">
      </script>
    </body>
  </html>