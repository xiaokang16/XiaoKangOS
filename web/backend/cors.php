<?php

function handleCors() {
    $origin = $_SERVER['HTTP_ORIGIN'] ?? '*';
    
    $allowedOrigins = [
        'http://localhost:3000',
        'http://localhost:8080',
        'http://127.0.0.1:3000',
        'http://127.0.0.1:8080',
        'http://localhost',
        'http://127.0.0.1'
    ];
    
    $host = $_SERVER['HTTP_HOST'] ?? '';
    $allowedOrigins[] = 'http://' . $host;
    $allowedOrigins[] = 'https://' . $host;
    
    if (in_array($origin, $allowedOrigins) || $origin === '*') {
        header("Access-Control-Allow-Origin: $origin");
    }
    
    header('Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS');
    header('Access-Control-Allow-Headers: Origin, Content-Type, Accept, Authorization, X-Requested-With, X-CSRF-Token');
    header('Access-Control-Allow-Credentials: true');
    header('Access-Control-Max-Age: 86400');
    
    if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
        http_response_code(204);
        exit;
    }
}

function addSecurityHeaders() {
    header('X-Content-Type-Options: nosniff');
    header('X-Frame-Options: DENY');
    header('X-XSS-Protection: 1; mode=block');
    header('Strict-Transport-Security: max-age=31536000; includeSubDomains');
    header('Content-Security-Policy: default-src \'self\'; script-src \'self\' \'unsafe-inline\'; style-src \'self\' \'unsafe-inline\'; img-src \'self\' data: https:; font-src \'self\' data:;');
    header('Referrer-Policy: strict-origin-when-cross-origin');
}
