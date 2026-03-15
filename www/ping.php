<?php
// ping.php - Program PHP Sederhana untuk membuktikan Eksekusi CGI Berhasil

header('Content-Type: text/plain');
echo "CGI Aktif! Waktu Server: " . date("H:i:s");
?>