const XiaoAPI = {
    _xiao: null,

    init() {
        this._xiao = window.xiao;
        if (!this._xiao) {
            console.warn('xiaojs API not available, running in demo mode');
            this._demoMode = true;
        }
    },

    async _safeCall(apiModule, method, ...args) {
        if (this._demoMode) {
            return this._getDemoData(apiModule, method, ...args);
        }

        try {
            const module = this._xiao[apiModule];
            if (!module || !module[method]) {
                throw new Error(`API method ${apiModule}.${method} not found`);
            }
            return await module[method](...args);
        } catch (error) {
            console.error(`Error calling ${apiModule}.${method}:`, error);
            throw error;
        }
    },

    _getDemoData(module, method, ...args) {
        const demoData = {
            system: {
                getInfo: async () => ({
                    osName: 'XiaoKangOS',
                    osVersion: '1.0.0',
                    kernelVersion: '5.15.0-xiaokang',
                    hostname: 'xiaokang-pc',
                    uptime: Math.floor(Math.random() * 86400 * 7),
                    platform: 'Web'
                }),
                getUptime: async () => Math.floor(Math.random() * 86400 * 7),
                getLoad: async () => ({
                    cpu: Math.random() * 100,
                    memory: Math.random() * 100,
                    load1: Math.random() * 2,
                    load5: Math.random() * 2,
                    load15: Math.random() * 2
                })
            },
            hardware: {
                getInfo: async () => ({
                    cpu: {
                        name: 'Intel Core i7-12700K',
                        cores: 12,
                        threads: 20,
                        frequency: 3600,
                        temperature: 45 + Math.random() * 30
                    },
                    gpu: {
                        name: 'NVIDIA RTX 4080',
                        memory: 16384,
                        usage: Math.random() * 100
                    },
                    memory: {
                        total: 32 * 1024 * 1024 * 1024,
                        used: Math.random() * 16 * 1024 * 1024 * 1024,
                        free: Math.random() * 16 * 1024 * 1024 * 1024
                    },
                    storage: [
                        {
                            name: 'SSD 1',
                            total: 1024 * 1024 * 1024 * 1024,
                            used: Math.random() * 512 * 1024 * 1024 * 1024,
                            type: 'SSD'
                        },
                        {
                            name: 'HDD 2',
                            total: 2 * 1024 * 1024 * 1024 * 1024,
                            used: Math.random() * 1024 * 1024 * 1024 * 1024,
                            type: 'HDD'
                        }
                    ],
                    display: {
                        resolution: '2560x1440',
                        refreshRate: 144,
                        count: 1
                    }
                })
            },
            fs: {
                readDir: async (path) => {
                    const demoFiles = {
                        '/': [
                            { name: 'home', type: 'directory', size: 0, modified: Date.now() - 86400000, permissions: 'rwxr-xr-x' },
                            { name: 'etc', type: 'directory', size: 0, modified: Date.now() - 172800000, permissions: 'rwxr-xr-x' },
                            { name: 'var', type: 'directory', size: 0, modified: Date.now() - 259200000, permissions: 'rwxr-xr-x' },
                            { name: 'usr', type: 'directory', size: 0, modified: Date.now() - 345600000, permissions: 'r-xr-xr-x' },
                            { name: 'bin', type: 'directory', size: 0, modified: Date.now() - 432000000, permissions: 'r-xr-xr-x' },
                            { name: 'boot', type: 'directory', size: 0, modified: Date.now() - 518400000, permissions: 'r-xr-xr-x' },
                            { name: 'readme.txt', type: 'file', size: 1024, modified: Date.now() - 60000, permissions: 'rw-r--r--' },
                            { name: 'config.json', type: 'file', size: 2048, modified: Date.now() - 120000, permissions: 'rw-r--r--' }
                        ],
                        '/home': [
                            { name: 'user', type: 'directory', size: 0, modified: Date.now() - 86400000, permissions: 'rwxr-xr-x' },
                            { name: 'guest', type: 'directory', size: 0, modified: Date.now() - 172800000, permissions: 'rwxr-xr-x' }
                        ],
                        '/home/user': [
                            { name: 'documents', type: 'directory', size: 0, modified: Date.now() - 86400000, permissions: 'rwxr-xr-x' },
                            { name: 'downloads', type: 'directory', size: 0, modified: Date.now() - 172800000, permissions: 'rwxr-xr-x' },
                            { name: 'desktop', type: 'directory', size: 0, modified: Date.now() - 259200000, permissions: 'rwxr-xr-x' },
                            { name: 'hello.txt', type: 'file', size: 512, modified: Date.now() - 60000, permissions: 'rw-r--r--' },
                            { name: 'notes.md', type: 'file', size: 4096, modified: Date.now() - 120000, permissions: 'rw-r--r--' }
                        ]
                    };
                    return demoFiles[path] || [];
                },
                getStats: async (path) => ({
                    size: Math.random() * 1024 * 1024,
                    created: Date.now() - Math.random() * 86400000 * 365,
                    modified: Date.now() - Math.random() * 86400000 * 30,
                    accessed: Date.now() - Math.random() * 86400000 * 7,
                    permissions: 'rw-r--r--'
                })
            },
            process: {
                list: async () => {
                    const processNames = ['systemd', 'kernel', 'init', 'bash', 'node', 'nginx', 'docker', 'postgres', 'redis', 'python', 'java', 'chrome', 'firefox', 'code', 'git', 'ssh', 'cron', 'logind', 'dbus', 'NetworkManager'];
                    const statuses = ['running', 'sleeping', 'stopped'];
                    const users = ['root', 'user', 'www-data', 'postgres', 'nobody'];
                    
                    return Array.from({ length: 20 }, (_, i) => ({
                        pid: 100 + i * 10,
                        name: processNames[i % processNames.length],
                        cpu: Math.random() * 30,
                        memory: Math.random() * 15,
                        status: statuses[Math.floor(Math.random() * statuses.length)],
                        user: users[Math.floor(Math.random() * users.length)],
                        startTime: Date.now() - Math.random() * 86400000
                    }));
                },
                kill: async (pid) => ({
                    success: Math.random() > 0.3,
                    pid
                })
            },
            network: {
                getStatus: async () => ({
                    connected: Math.random() > 0.1,
                    ip: '192.168.1.' + Math.floor(Math.random() * 255),
                    gateway: '192.168.1.1',
                    dns: ['8.8.8.8', '8.8.4.4'],
                    interface: 'eth0'
                }),
                getStats: async () => ({
                    upload: Math.random() * 1024 * 1024,
                    download: Math.random() * 10 * 1024 * 1024
                })
            }
        };

        const moduleData = demoData[module];
        if (moduleData && moduleData[method]) {
            return moduleData[method](...args);
        }
        
        return null;
    },

    async systemGetInfo() {
        return this._safeCall('system', 'getInfo');
    },

    async systemGetUptime() {
        return this._safeCall('system', 'getUptime');
    },

    async systemGetLoad() {
        return this._safeCall('system', 'getLoad');
    },

    async hardwareGetInfo() {
        return this._safeCall('hardware', 'getInfo');
    },

    async fsReadDir(path) {
        return this._safeCall('fs', 'readDir', path);
    },

    async fsGetStats(path) {
        return this._safeCall('fs', 'getStats', path);
    },

    async processList() {
        return this._safeCall('process', 'list');
    },

    async processKill(pid) {
        return this._safeCall('process', 'kill', pid);
    },

    async networkGetStatus() {
        return this._safeCall('network', 'getStatus');
    },

    async networkGetStats() {
        return this._safeCall('network', 'getStats');
    }
};

if (typeof module !== 'undefined' && module.exports) {
    module.exports = XiaoAPI;
}
