<?php
// Mengambil meta informasi lingkungan server dengan aman kelak 
$server_software = isset($_SERVER['SERVER_SOFTWARE']) ? $_SERVER['SERVER_SOFTWARE'] : 'Unknown Server';
$php_version = phpversion();
$protocol = isset($_SERVER['SERVER_PROTOCOL']) ? $_SERVER['SERVER_PROTOCOL'] : 'HTTP/1.1';
$doc_root = isset($_SERVER['DOCUMENT_ROOT']) ? $_SERVER['DOCUMENT_ROOT'] : '/var/www';

// Mendeteksi apakah kita berjalan di atas terowongan HTTPS atau bukan
$is_https = (isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] === 'on') || 
            (isset($_SERVER['SERVER_PORT']) && $_SERVER['SERVER_PORT'] == 8443) 
            ? 'Aktif (Aman)' : 'Tidak Aktif';
?>
<!DOCTYPE html>
<html lang="id">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Safa Web Server - Berjalan!</title>
    <!-- Modern Typography: Google Fonts "Outfit" -->
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;800&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-color: #0d1117;
            --text-primary: #e6edf3;
            --text-secondary: #8b949e;
            --accent: #2f81f7;
            --glass-bg: rgba(22, 27, 34, 0.6);
            --glass-border: rgba(255, 255, 255, 0.1);
        }

        body {
            margin: 0;
            padding: 0;
            font-family: 'Outfit', sans-serif;
            background-color: var(--bg-color);
            color: var(--text-primary);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            overflow: hidden;
        }

        /* Latar Belakang Orb / Blur Lights Dinamis */
        .background-orbs {
            position: absolute;
            width: 100%;
            height: 100%;
            z-index: 0;
            overflow: hidden;
        }

        .orb {
            position: absolute;
            border-radius: 50%;
            filter: blur(80px);
            opacity: 0.5;
            animation: float 20s infinite ease-in-out alternate;
        }

        .orb-1 {
            width: 400px;
            height: 400px;
            background: radial-gradient(circle, #2f81f7, #1f6feb);
            top: -100px;
            left: 10%;
        }

        .orb-2 {
            width: 500px;
            height: 500px;
            background: radial-gradient(circle, #8957e5, #d2a8ff);
            bottom: -150px;
            right: 10%;
            animation-delay: -5s;
        }

        @keyframes float {
            0% { transform: translate(0, 0) scale(1); }
            100% { transform: translate(50px, 50px) scale(1.1); }
        }

        /* Glassmorphism Container */
        .glass-container {
            position: relative;
            z-index: 1;
            background: var(--glass-bg);
            backdrop-filter: blur(20px);
            -webkit-backdrop-filter: blur(20px);
            border: 1px solid var(--glass-border);
            border-radius: 24px;
            padding: 3rem;
            width: 90%;
            max-width: 650px;
            box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.5);
            transform: translateY(20px);
            opacity: 0;
            animation: appear 0.8s forwards cubic-bezier(0.16, 1, 0.3, 1);
        }

        @keyframes appear {
            to { transform: translateY(0); opacity: 1; }
        }

        h1 {
            font-size: 2.5rem;
            font-weight: 800;
            margin: 0 0 0.5rem 0;
            background: linear-gradient(135deg, #e6edf3 0%, #8b949e 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }

        p.subtitle {
            color: var(--text-secondary);
            font-size: 1.1rem;
            margin-top: 0;
            margin-bottom: 2.5rem;
            line-height: 1.5;
        }

        .info-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 1.5rem;
        }

        .info-card {
            background: rgba(255, 255, 255, 0.03);
            border: 1px solid rgba(255, 255, 255, 0.05);
            border-radius: 16px;
            padding: 1.5rem;
            transition: all 0.3s ease;
        }

        .info-card:hover {
            transform: translateY(-5px);
            background: rgba(255, 255, 255, 0.05);
            border-color: rgba(255, 255, 255, 0.15);
            box-shadow: 0 10px 20px rgba(0,0,0,0.2);
        }

        .info-label {
            font-size: 0.85rem;
            color: var(--text-secondary);
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-bottom: 0.5rem;
            font-weight: 600;
        }

        .info-value {
            font-size: 1.25rem;
            font-weight: 600;
            color: var(--text-primary);
        }

        .info-value.highlight {
            color: #d2a8ff;
            text-shadow: 0 0 10px rgba(210, 168, 255, 0.4);
        }

        .info-value.green {
            color: #3fb950;
            text-shadow: 0 0 10px rgba(63, 185, 80, 0.4);
        }

        .footer {
            margin-top: 2.5rem;
            text-align: center;
            font-size: 0.95rem;
            color: var(--text-secondary);
            border-top: 1px solid rgba(255, 255, 255, 0.1);
            padding-top: 1.5rem;
        }

        @media (max-width: 600px) {
            .info-grid {
                grid-template-columns: 1fr;
            }
        }
    </style>
</head>
<body>
    <div class="background-orbs">
        <div class="orb orb-1"></div>
        <div class="orb orb-2"></div>
    </div>
    
    <div class="glass-container">
        <h1>Safa Web Server</h1>
        <p class="subtitle">Semuanya telah dikonfigurasi dan berfungsi dengan sangat baik.<br>Sistem web server C++ pribadi Anda sudah melayani trafik.</p>
        
        <div class="info-grid">
            <div class="info-card">
                <div class="info-label">Versi Mesin Server</div>
                <div class="info-value highlight"><?= htmlspecialchars($server_software) ?></div>
            </div>
            
            <div class="info-card">
                <div class="info-label">Mesin Eksekutor CGI</div>
                <div class="info-value">PHP <?= htmlspecialchars($php_version) ?></div>
            </div>

            <div class="info-card">
                <div class="info-label">Protokol Permintaan</div>
                <div class="info-value"><?= htmlspecialchars($protocol) ?></div>
            </div>

            <div class="info-card">
                <div class="info-label">Enkripsi OpenSSL</div>
                <div class="info-value <?= $is_https !== 'Tidak Aktif' ? 'green' : '' ?>"><?= htmlspecialchars($is_https) ?></div>
            </div>
        </div>

        <div class="footer">
            Direktori Dokumen Induk (Root): <strong><?= htmlspecialchars($doc_root) ?></strong>
        </div>
    </div>
</body>
</html>
