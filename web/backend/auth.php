<?php

require_once __DIR__ . '/config.php';
require_once __DIR__ . '/jwt.php';

function handleRegister() {
    requireMethod('POST');
    
    $rawInput = file_get_contents('php://input');
    $data = json_decode($rawInput, true);
    
    if (!$data) {
        $data = $_POST;
    }
    
    $username = sanitizeInput($data['username'] ?? '');
    $email = sanitizeInput($data['email'] ?? '');
    $password = $data['password'] ?? '';
    $displayName = sanitizeInput($data['display_name'] ?? '');
    $csrfToken = $data['csrf_token'] ?? '';
    
    if (empty($username) || empty($email) || empty($password)) {
        jsonResponse(['error' => '用户名、邮箱和密码为必填项'], 400);
    }
    
    if (!validateUsername($username)) {
        jsonResponse([
            'error' => '用户名格式不正确',
            'details' => '用户名需要 ' . USERNAME_MIN_LENGTH . '-' . USERNAME_MAX_LENGTH . ' 个字符，只能包含字母、数字和下划线'
        ], 400);
    }
    
    if (!validateEmail($email)) {
        jsonResponse(['error' => '邮箱格式不正确'], 400);
    }
    
    if (!validatePassword($password)) {
        jsonResponse([
            'error' => '密码强度不足',
            'details' => '密码至少需要 ' . PASSWORD_MIN_LENGTH . ' 个字符'
        ], 400);
    }
    
    $pdo = getDb();
    
    $stmt = $pdo->prepare("SELECT id FROM users WHERE username = ?");
    $stmt->execute([$username]);
    if ($stmt->fetch()) {
        jsonResponse(['error' => '用户名已存在'], 409);
    }
    
    $stmt = $pdo->prepare("SELECT id FROM users WHERE email = ?");
    $stmt->execute([$email]);
    if ($stmt->fetch()) {
        jsonResponse(['error' => '邮箱已被注册'], 409);
    }
    
    $passwordHash = password_hash($password, PASSWORD_BCRYPT, ['cost' => 12]);
    
    if (empty($displayName)) {
        $displayName = $username;
    }
    
    try {
        $stmt = $pdo->prepare("
            INSERT INTO users (username, email, password_hash, display_name, role, created_at)
            VALUES (?, ?, ?, ?, 'user', datetime('now'))
        ");
        $stmt->execute([$username, $email, $passwordHash, $displayName]);
        
        $userId = $pdo->lastInsertId();
        
        $token = generateAccessToken($userId, $username, 'user');
        
        $ipAddress = $_SERVER['REMOTE_ADDR'] ?? '';
        $userAgent = $_SERVER['HTTP_USER_AGENT'] ?? '';
        $tokenHash = hash('sha256', $token);
        $expiresAt = date('Y-m-d H:i:s', time() + JWT_EXPIRY);
        
        $sessionStmt = $pdo->prepare("
            INSERT INTO sessions (user_id, token_hash, expires_at, ip_address, user_agent)
            VALUES (?, ?, ?, ?, ?)
        ");
        $sessionStmt->execute([$userId, $tokenHash, $expiresAt, $ipAddress, $userAgent]);
        
        logError("用户注册成功", ['user_id' => $userId, 'username' => $username]);
        
        jsonResponse([
            'message' => '注册成功',
            'user' => [
                'id' => $userId,
                'username' => $username,
                'email' => $email,
                'display_name' => $displayName,
                'role' => 'user'
            ],
            'token' => $token
        ], 201);
        
    } catch (PDOException $e) {
        logError("注册失败: " . $e->getMessage());
        jsonResponse(['error' => '注册失败，请稍后重试'], 500);
    }
}

function handleLogin() {
    requireMethod('POST');
    
    $rawInput = file_get_contents('php://input');
    $data = json_decode($rawInput, true);
    
    if (!$data) {
        $data = $_POST;
    }
    
    $username = sanitizeInput($data['username'] ?? '');
    $password = $data['password'] ?? '';
    
    if (empty($username) || empty($password)) {
        jsonResponse(['error' => '用户名和密码为必填项'], 400);
    }
    
    $pdo = getDb();
    
    $stmt = $pdo->prepare("
        SELECT id, username, email, password_hash, display_name, role, is_active
        FROM users WHERE username = ? OR email = ?
    ");
    $stmt->execute([$username, $username]);
    $user = $stmt->fetch();
    
    if (!$user) {
        jsonResponse(['error' => '用户名或密码错误'], 401);
    }
    
    if (!$user['is_active']) {
        jsonResponse(['error' => '账户已被禁用'], 403);
    }
    
    if (!password_verify($password, $user['password_hash'])) {
        logError("登录失败 - 密码错误", ['user_id' => $user['id'], 'username' => $user['username']]);
        jsonResponse(['error' => '用户名或密码错误'], 401);
    }
    
    $token = generateAccessToken($user['id'], $user['username'], $user['role']);
    
    $ipAddress = $_SERVER['REMOTE_ADDR'] ?? '';
    $userAgent = $_SERVER['HTTP_USER_AGENT'] ?? '';
    $tokenHash = hash('sha256', $token);
    $expiresAt = date('Y-m-d H:i:s', time() + JWT_EXPIRY);
    
    $sessionStmt = $pdo->prepare("
        INSERT INTO sessions (user_id, token_hash, expires_at, ip_address, user_agent)
        VALUES (?, ?, ?, ?, ?)
    ");
    $sessionStmt->execute([$user['id'], $tokenHash, $expiresAt, $ipAddress, $userAgent]);
    
    $updateStmt = $pdo->prepare("UPDATE users SET last_login = datetime('now') WHERE id = ?");
    $updateStmt->execute([$user['id']]);
    
    logError("用户登录成功", ['user_id' => $user['id'], 'username' => $user['username']]);
    
    jsonResponse([
        'message' => '登录成功',
        'user' => [
            'id' => $user['id'],
            'username' => $user['username'],
            'email' => $user['email'],
            'display_name' => $user['display_name'],
            'role' => $user['role']
        ],
        'token' => $token
    ]);
}

function handleLogout() {
    requireMethod('POST');
    
    $token = getBearerToken();
    
    if ($token) {
        $pdo = getDb();
        $tokenHash = hash('sha256', $token);
        
        $stmt = $pdo->prepare("DELETE FROM sessions WHERE token_hash = ?");
        $stmt->execute([$tokenHash]);
    }
    
    jsonResponse(['message' => '登出成功']);
}

function handleRefreshToken() {
    requireMethod('POST');
    
    $user = requireAuth();
    
    $pdo = getDb();
    
    $stmt = $pdo->prepare("SELECT username, role FROM users WHERE id = ?");
    $stmt->execute([$user['sub']]);
    $userData = $stmt->fetch();
    
    if (!$userData) {
        jsonResponse(['error' => '用户不存在'], 404);
    }
    
    $token = generateAccessToken($user['sub'], $userData['username'], $userData['role']);
    
    $tokenHash = hash('sha256', $token);
    $expiresAt = date('Y-m-d H:i:s', time() + JWT_EXPIRY);
    
    $oldTokenHash = hash('sha256', getBearerToken());
    $stmt = $pdo->prepare("DELETE FROM sessions WHERE token_hash = ?");
    $stmt->execute([$oldTokenHash]);
    
    $ipAddress = $_SERVER['REMOTE_ADDR'] ?? '';
    $userAgent = $_SERVER['HTTP_USER_AGENT'] ?? '';
    
    $sessionStmt = $pdo->prepare("
        INSERT INTO sessions (user_id, token_hash, expires_at, ip_address, user_agent)
        VALUES (?, ?, ?, ?, ?)
    ");
    $sessionStmt->execute([$user['sub'], $tokenHash, $expiresAt, $ipAddress, $userAgent]);
    
    jsonResponse([
        'message' => '令牌刷新成功',
        'token' => $token
    ]);
}

function handleCsrfToken() {
    requireMethod('GET');
    
    $user = getCurrentUser();
    
    if ($user) {
        $_SESSION['user_id'] = $user['sub'];
    }
    
    $token = generateCsrfToken();
    
    jsonResponse([
        'csrf_token' => $token
    ]);
}

function handleAuthRoute($action) {
    switch ($action) {
        case 'register':
            handleRegister();
            break;
        case 'login':
            handleLogin();
            break;
        case 'logout':
            handleLogout();
            break;
        case 'refresh':
            handleRefreshToken();
            break;
        case 'csrf':
            handleCsrfToken();
            break;
        default:
            jsonResponse(['error' => '无效的认证端点'], 404);
    }
}
