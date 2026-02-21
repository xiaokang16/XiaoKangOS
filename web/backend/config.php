<?php

define('DB_PATH', __DIR__ . '/../data/xiaokangos.db');
define('JWT_SECRET', 'xiaokangOS@2024#SecureToken!ChangeInProduction');
define('JWT_ALGORITHM', 'HS256');
define('JWT_EXPIRY', 86400);
define('CSRF_TOKEN_NAME', 'csrf_token');
define('SESSION_NAME', 'XKOS_SESSION');

define('API_VERSION', 'v1');
define('BASE_URL', '/api');

define('PASSWORD_MIN_LENGTH', 8);
define('USERNAME_MIN_LENGTH', 3);
define('USERNAME_MAX_LENGTH', 50);

error_reporting(E_ALL);
ini_set('display_errors', 0);
ini_set('log_errors', 1);
ini_set('error_log', __DIR__ . '/../logs/error.log');

header('Content-Type: application/json; charset=utf-8');

function getDb() {
    static $pdo = null;
    
    if ($pdo === null) {
        $dbDir = dirname(DB_PATH);
        if (!is_dir($dbDir)) {
            mkdir($dbDir, 0755, true);
        }
        
        try {
            $pdo = new PDO('sqlite:' . DB_PATH);
            $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
            $pdo->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_ASSOC);
            initDatabase($pdo);
        } catch (PDOException $e) {
            http_response_code(500);
            echo json_encode(['error' => '数据库连接失败']);
            exit;
        }
    }
    
    return $pdo;
}

function initDatabase($pdo) {
    $pdo->exec("
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            email TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            display_name TEXT,
            avatar_url TEXT,
            role TEXT DEFAULT 'user',
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            last_login DATETIME,
            is_active INTEGER DEFAULT 1
        )
    ");
    
    $pdo->exec("
        CREATE TABLE IF NOT EXISTS sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            token_hash TEXT UNIQUE NOT NULL,
            expires_at DATETIME NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            ip_address TEXT,
            user_agent TEXT,
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
        )
    ");
    
    $pdo->exec("
        CREATE TABLE IF NOT EXISTS csrf_tokens (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            token TEXT UNIQUE NOT NULL,
            user_id INTEGER,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            expires_at DATETIME NOT NULL,
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
        )
    ");
    
    $pdo->exec("CREATE INDEX IF NOT EXISTS idx_sessions_user ON sessions(user_id)");
    $pdo->exec("CREATE INDEX IF NOT EXISTS idx_sessions_token ON sessions(token_hash)");
    $pdo->exec("CREATE INDEX IF NOT EXISTS idx_users_username ON users(username)");
    $pdo->exec("CREATE INDEX IF NOT EXISTS idx_users_email ON users(email)");
}

function sanitizeInput($input) {
    if (is_array($input)) {
        return array_map('sanitizeInput', $input);
    }
    return htmlspecialchars(trim($input), ENT_QUOTES, 'UTF-8');
}

function validateEmail($email) {
    return filter_var($email, FILTER_VALIDATE_EMAIL) !== false;
}

function validateUsername($username) {
    return preg_match('/^[a-zA-Z0-9_]{' . USERNAME_MIN_LENGTH . ',' . USERNAME_MAX_LENGTH . '}$/', $username);
}

function validatePassword($password) {
    return strlen($password) >= PASSWORD_MIN_LENGTH;
}

function generateCsrfToken() {
    $token = bin2hex(random_bytes(32));
    $pdo = getDb();
    $expires = date('Y-m-d H:i:s', time() + 3600);
    
    $userId = $_SESSION['user_id'] ?? null;
    
    $stmt = $pdo->prepare("INSERT INTO csrf_tokens (token, user_id, expires_at) VALUES (?, ?, ?)");
    $stmt->execute([$token, $userId, $expires]);
    
    return $token;
}

function validateCsrfToken($token) {
    if (empty($token)) {
        return false;
    }
    
    $pdo = getDb();
    $now = date('Y-m-d H:i:s');
    
    $stmt = $pdo->prepare("SELECT id FROM csrf_tokens WHERE token = ? AND expires_at > ?");
    $stmt->execute([$token, $now]);
    $result = $stmt->fetch();
    
    if ($result) {
        $deleteStmt = $pdo->prepare("DELETE FROM csrf_tokens WHERE token = ?");
        $deleteStmt->execute([$token]);
        return true;
    }
    
    return false;
}

function logError($message, $context = []) {
    $logDir = dirname(__DIR__) . '/logs';
    if (!is_dir($logDir)) {
        mkdir($logDir, 0755, true);
    }
    
    $logFile = $logDir . '/error.log';
    $timestamp = date('Y-m-d H:i:s');
    $contextStr = !empty($context) ? json_encode($context) : '';
    $logMessage = "[$timestamp] $message $contextStr\n";
    
    error_log($logMessage, 3, $logFile);
}

function jsonResponse($data, $statusCode = 200) {
    http_response_code($statusCode);
    echo json_encode($data, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
    exit;
}

function requireMethod($method) {
    if ($_SERVER['REQUEST_METHOD'] !== $method) {
        jsonResponse(['error' => '方法不允许'], 405);
    }
}

function getBearerToken() {
    $headers = getallheaders();
    $authHeader = $headers['Authorization'] ?? $headers['authorization'] ?? '';
    
    if (preg_match('/Bearer\s+(.+)$/i', $authHeader, $matches)) {
        return $matches[1];
    }
    
    return null;
}
