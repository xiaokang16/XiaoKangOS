<?php

require_once __DIR__ . '/config.php';

class JWT {
    private static $secret = JWT_SECRET;
    private static $algorithm = JWT_ALGORITHM;
    
    public static function encode($payload) {
        $header = [
            'typ' => 'JWT',
            'alg' => self::$algorithm
        ];
        
        $headerEncoded = self::base64UrlEncode(json_encode($header));
        $payloadEncoded = self::base64UrlEncode(json_encode($payload));
        
        $signature = self::sign("$headerEncoded.$payloadEncoded");
        $signatureEncoded = self::base64UrlEncode($signature);
        
        return "$headerEncoded.$payloadEncoded.$signatureEncoded";
    }
    
    public static function decode($token) {
        $parts = explode('.', $token);
        
        if (count($parts) !== 3) {
            return null;
        }
        
        list($headerEncoded, $payloadEncoded, $signatureEncoded) = $parts;
        
        $signature = self::sign("$headerEncoded.$payloadEncoded");
        $expectedSignature = self::base64UrlEncode($signature);
        
        if (!hash_equals($signatureEncoded, $expectedSignature)) {
            return null;
        }
        
        $header = json_decode(self::base64UrlDecode($headerEncoded), true);
        if (!$header || $header['alg'] !== self::$algorithm) {
            return null;
        }
        
        $payload = json_decode(self::base64UrlDecode($payloadEncoded), true);
        
        if (isset($payload['exp']) && $payload['exp'] < time()) {
            return null;
        }
        
        if (isset($payload['nbf']) && $payload['nbf'] > time()) {
            return null;
        }
        
        return $payload;
    }
    
    public static function validate($token) {
        return self::decode($token) !== null;
    }
    
    public static function createToken($userId, $username, $role = 'user', $extraClaims = []) {
        $now = time();
        
        $payload = [
            'iss' => 'xiaokangOS',
            'aud' => 'xiaokangOS',
            'iat' => $now,
            'nbf' => $now,
            'exp' => $now + JWT_EXPIRY,
            'sub' => (string)$userId,
            'username' => $username,
            'role' => $role
        ];
        
        $payload = array_merge($payload, $extraClaims);
        
        return self::encode($payload);
    }
    
    public static function getPayload($token) {
        return self::decode($token);
    }
    
    private static function base64UrlEncode($data) {
        return rtrim(strtr(base64_encode($data), '+/', '-_'), '=');
    }
    
    private static function base64UrlDecode($data) {
        $remainder = strlen($data) % 4;
        if ($remainder) {
            $data .= str_repeat('=', 4 - $remainder);
        }
        return base64_decode(strtr($data, '-_', '+/'));
    }
    
    private static function sign($data) {
        return hash_hmac('sha256', $data, self::$secret, true);
    }
}

function generateAccessToken($userId, $username, $role = 'user') {
    return JWT::createToken($userId, $username, $role);
}

function validateAccessToken($token) {
    $payload = JWT::decode($token);
    
    if (!$payload) {
        return null;
    }
    
    return $payload;
}

function getCurrentUser() {
    $token = getBearerToken();
    
    if (!$token) {
        return null;
    }
    
    return validateAccessToken($token);
}

function requireAuth() {
    $user = getCurrentUser();
    
    if (!$user) {
        jsonResponse(['error' => '未授权访问'], 401);
    }
    
    return $user;
}

function requireRole($role) {
    $user = requireAuth();
    
    $roles = [
        'admin' => 3,
        'moderator' => 2,
        'user' => 1
    ];
    
    $userLevel = $roles[$user['role']] ?? 0;
    $requiredLevel = $roles[$role] ?? 0;
    
    if ($userLevel < $requiredLevel) {
        jsonResponse(['error' => '权限不足'], 403);
    }
    
    return $user;
}
