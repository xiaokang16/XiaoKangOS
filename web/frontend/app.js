class XiaoKangOS {
    constructor() {
        this.state = {
            currentSection: 'dashboard',
            currentPath: '/',
            pathHistory: ['/'],
            processes: [],
            selectedProcess: null,
            selectedFile: null,
            refreshInterval: 2000,
            autoRefresh: true,
            animations: true,
            systemInfo: null,
            hardwareInfo: null,
            networkStatus: null,
            networkStats: null
        };

        this.intervals = {
            time: null,
            refresh: null
        };

        this.init();
    }

    async init() {
        XiaoAPI.init();
        this.bindEvents();
        await this.initializeApp();
    }

    bindEvents() {
        document.querySelectorAll('.nav-tab').forEach(tab => {
            tab.addEventListener('click', (e) => {
                const section = e.currentTarget.dataset.section;
                this.navigateTo(section);
            });
        });

        document.getElementById('btn-back')?.addEventListener('click', () => this.navigateBack());
        document.getElementById('btn-refresh')?.addEventListener('click', () => this.refreshFiles());
        document.getElementById('refresh-processes')?.addEventListener('click', () => this.refreshProcesses());
        document.getElementById('kill-selected')?.addEventListener('click', () => this.killSelectedProcess());

        document.getElementById('auto-refresh')?.addEventListener('change', (e) => {
            this.state.autoRefresh = e.target.checked;
            this.updateRefreshInterval();
        });

        document.getElementById('refresh-interval')?.addEventListener('change', (e) => {
            this.state.refreshInterval = parseInt(e.target.value) * 1000;
            this.updateRefreshInterval();
        });

        document.getElementById('animations')?.addEventListener('change', (e) => {
            this.state.animations = e.target.checked;
            document.body.classList.toggle('no-animations', !e.target.checked);
        });
    }

    async initializeApp() {
        this.startClock();
        await this.loadSystemInfo();
        await this.loadDashboard();
        this.startAutoRefresh();
        this.showToast('欢迎使用 XiaoKangOS', 'success');
    }

    startClock() {
        const updateTime = () => {
            const now = new Date();
            const timeStr = now.toLocaleTimeString('zh-CN', { hour12: false });
            const timeEl = document.getElementById('system-time');
            if (timeEl) timeEl.textContent = timeStr;
        };

        updateTime();
        this.intervals.time = setInterval(updateTime, 1000);
    }

    startAutoRefresh() {
        this.intervals.refresh = setInterval(() => {
            if (this.state.autoRefresh) {
                this.loadDashboard();
            }
        }, this.state.refreshInterval);
    }

    updateRefreshInterval() {
        if (this.intervals.refresh) {
            clearInterval(this.intervals.refresh);
        }
        if (this.state.autoRefresh) {
            this.startAutoRefresh();
        }
    }

    navigateTo(section) {
        this.state.currentSection = section;

        document.querySelectorAll('.nav-tab').forEach(tab => {
            tab.classList.toggle('active', tab.dataset.section === section);
        });

        document.querySelectorAll('.section').forEach(sec => {
            sec.classList.toggle('active', sec.id === section);
        });

        if (section === 'files') {
            this.loadFiles(this.state.currentPath);
        } else if (section === 'processes') {
            this.loadProcesses();
        }
    }

    async loadSystemInfo() {
        try {
            this.state.systemInfo = await XiaoAPI.systemGetInfo();
            this.state.hardwareInfo = await XiaoAPI.hardwareGetInfo();
            this.state.networkStatus = await XiaoAPI.networkGetStatus();
        } catch (error) {
            console.error('Failed to load system info:', error);
            this.showToast('加载系统信息失败', 'error');
        }
    }

    async loadDashboard() {
        try {
            const [load, networkStatus, networkStats] = await Promise.all([
                XiaoAPI.systemGetLoad(),
                XiaoAPI.networkGetStatus(),
                XiaoAPI.networkGetStats()
            ]);

            this.state.networkStats = networkStats;
            this.updateDashboard(load, networkStatus);
        } catch (error) {
            console.error('Failed to load dashboard:', error);
        }
    }

    updateDashboard(load, networkStatus) {
        this.updateSystemInfo();
        this.updateCpuGauge(load.cpu);
        this.updateMemoryGauge(load.memory);
        this.updateNetworkStatus(networkStatus);
        this.updateStorageBars();
        this.updateProcessStats();
    }

    async updateSystemInfo() {
        if (!this.state.systemInfo) return;

        const info = this.state.systemInfo;

        document.getElementById('kernel-version').textContent = info.kernelVersion || '-';
        document.getElementById('hostname').textContent = info.hostname || '-';
        document.getElementById('uptime').textContent = this.formatUptime(info.uptime);

        if (this.state.hardwareInfo) {
            const hw = this.state.hardwareInfo;
            
            document.getElementById('hardware-cpu').textContent = hw.cpu?.name || '-';
            document.getElementById('hardware-gpu').textContent = hw.gpu?.name || '-';
            document.getElementById('hardware-display').textContent = hw.display?.resolution || '-';

            if (hw.memory) {
                document.getElementById('total-memory').textContent = this.formatBytes(hw.memory.total);
                document.getElementById('used-memory').textContent = this.formatBytes(hw.memory.used);
                document.getElementById('free-memory').textContent = this.formatBytes(hw.memory.free);
            }
        }

        document.getElementById('cpu-usage').textContent = `${Math.round(load?.cpu || 0)}%`;
        document.getElementById('memory-usage').textContent = `${Math.round(load?.memory || 0)}%`;
    }

    updateCpuGauge(percentage) {
        const gauge = document.getElementById('cpu-gauge');
        const text = document.getElementById('cpu-gauge-text');
        
        if (gauge && text) {
            const offset = 314 - (314 * percentage / 100);
            gauge.style.strokeDashoffset = offset;
            text.textContent = `${Math.round(percentage)}%`;
        }
    }

    updateMemoryGauge(percentage) {
        const gauge = document.getElementById('memory-gauge');
        const text = document.getElementById('memory-gauge-text');
        
        if (gauge && text) {
            const offset = 314 - (314 * percentage / 100);
            gauge.style.strokeDashoffset = offset;
            text.textContent = `${Math.round(percentage)}%`;
        }
    }

    async updateStorageBars() {
        const container = document.getElementById('storage-bars');
        if (!container || !this.state.hardwareInfo?.storage) return;

        container.innerHTML = this.state.hardwareInfo.storage.map(storage => {
            const usedPercent = (storage.used / storage.total) * 100;
            let statusClass = 'used';
            if (usedPercent > 90) statusClass = 'danger';
            else if (usedPercent > 70) statusClass = 'warning';

            return `
                <div class="storage-item">
                    <div class="storage-label">
                        <span>${storage.name} (${storage.type})</span>
                        <span>${this.formatBytes(storage.used)} / ${this.formatBytes(storage.total)}</span>
                    </div>
                    <div class="storage-bar">
                        <div class="storage-fill ${statusClass}" style="width: ${usedPercent}%"></div>
                    </div>
                </div>
            `;
        }).join('');
    }

    updateNetworkStatus(status) {
        const statusEl = document.getElementById('network-status-text');
        const netConnection = document.getElementById('net-connection');
        const ipAddress = document.getElementById('ip-address');
        const uploadSpeed = document.getElementById('upload-speed');
        const downloadSpeed = document.getElementById('download-speed');

        if (statusEl) {
            statusEl.textContent = status.connected ? '在线' : '离线';
            statusEl.style.color = status.connected ? 'var(--accent-success)' : 'var(--accent-danger)';
        }

        if (netConnection) {
            netConnection.textContent = status.connected ? '已连接' : '未连接';
            netConnection.className = `info-value network-status ${status.connected ? 'connected' : 'disconnected'}`;
        }

        if (ipAddress) {
            ipAddress.textContent = status.ip || '-';
        }

        if (this.state.networkStats) {
            if (uploadSpeed) {
                uploadSpeed.textContent = this.formatSpeed(this.state.networkStats.upload);
            }
            if (downloadSpeed) {
                downloadSpeed.textContent = this.formatSpeed(this.state.networkStats.download);
            }
        }
    }

    async loadFiles(path) {
        const fileList = document.getElementById('file-list');
        if (!fileList) return;

        fileList.innerHTML = '<div class="loading">加载中...</div>';
        this.state.currentPath = path;

        try {
            const files = await XiaoAPI.fsReadDir(path);
            this.renderFileList(files);
            this.updateBreadcrumb(path);
        } catch (error) {
            console.error('Failed to load files:', error);
            fileList.innerHTML = '<div class="empty-message"><span>📂</span><p>无法加载文件列表</p></div>';
            this.showToast('加载文件失败', 'error');
        }
    }

    renderFileList(files) {
        const fileList = document.getElementById('file-list');
        if (!fileList) return;

        if (!files || files.length === 0) {
            fileList.innerHTML = '<div class="empty-message"><span>📂</span><p>空目录</p></div>';
            return;
        }

        const sortedFiles = [...files].sort((a, b) => {
            if (a.type === 'directory' && b.type !== 'directory') return -1;
            if (a.type !== 'directory' && b.type === 'directory') return 1;
            return a.name.localeCompare(b.name);
        });

        fileList.innerHTML = sortedFiles.map(file => `
            <div class="file-item ${file.type}" data-name="${file.name}" data-type="${file.type}" data-path="${this.state.currentPath}/${file.name}">
                <div class="file-name">
                    <span class="file-icon">${file.type === 'directory' ? '📁' : this.getFileIcon(file.name)}</span>
                    <span>${file.name}</span>
                </div>
                <div class="file-size">${file.type === 'directory' ? '-' : this.formatBytes(file.size)}</div>
                <div class="file-modified">${this.formatDate(file.modified)}</div>
                <div class="file-permissions">${file.permissions}</div>
            </div>
        `).join('');

        fileList.querySelectorAll('.file-item').forEach(item => {
            item.addEventListener('click', () => this.handleFileClick(item));
            item.addEventListener('dblclick', () => this.handleFileDoubleClick(item));
        });
    }

    getFileIcon(filename) {
        const ext = filename.split('.').pop()?.toLowerCase();
        const icons = {
            txt: '📄',
            md: '📝',
            json: '📋',
            js: '📜',
            html: '🌐',
            css: '🎨',
            png: '🖼️',
            jpg: '🖼️',
            jpeg: '🖼️',
            gif: '🖼️',
            mp3: '🎵',
            mp4: '🎬',
            zip: '📦',
            rar: '📦',
            pdf: '📕',
            exe: '⚙️'
        };
        return icons[ext] || '📄';
    }

    handleFileClick(item) {
        document.querySelectorAll('.file-item').forEach(el => el.classList.remove('selected'));
        item.classList.add('selected');

        this.state.selectedFile = {
            name: item.dataset.name,
            type: item.dataset.type,
            path: item.dataset.path
        };

        this.updateFileDetails(this.state.selectedFile);
    }

    async handleFileDoubleClick(item) {
        const type = item.dataset.type;
        const name = item.dataset.name;
        const path = item.dataset.path;

        if (type === 'directory') {
            this.state.pathHistory.push(path);
            await this.loadFiles(path);
        } else {
            this.showToast(`打开文件: ${name}`, 'info');
        }
    }

    async updateFileDetails(file) {
        if (!file) return;

        document.getElementById('detail-name').textContent = file.name;
        document.getElementById('detail-path').textContent = file.path;
        document.getElementById('detail-type').textContent = file.type === 'directory' ? '目录' : '文件';

        try {
            const stats = await XiaoAPI.fsGetStats(file.path);
            document.getElementById('detail-size').textContent = this.formatBytes(stats.size);
            document.getElementById('detail-permissions').textContent = stats.permissions;
        } catch (error) {
            document.getElementById('detail-size').textContent = '-';
            document.getElementById('detail-permissions').textContent = '-';
        }
    }

    async navigateBack() {
        if (this.state.pathHistory.length > 1) {
            this.state.pathHistory.pop();
            const previousPath = this.state.pathHistory[this.state.pathHistory.length - 1];
            await this.loadFiles(previousPath);
        }
    }

    async refreshFiles() {
        await this.loadFiles(this.state.currentPath);
        this.showToast('文件列表已刷新', 'success');
    }

    updateBreadcrumb(path) {
        const breadcrumb = document.getElementById('breadcrumb');
        if (!breadcrumb) return;

        const parts = path.split('/').filter(Boolean);
        let currentPath = '';

        let html = '<span class="breadcrumb-item" data-path="/">根目录</span>';

        parts.forEach((part, index) => {
            currentPath += '/' + part;
            html += `<span class="breadcrumb-separator">/</span>`;
            html += `<span class="breadcrumb-item ${index === parts.length - 1 ? 'active' : ''}" data-path="${currentPath}">${part}</span>`;
        });

        breadcrumb.innerHTML = html;

        breadcrumb.querySelectorAll('.breadcrumb-item').forEach(item => {
            item.addEventListener('click', () => {
                const itemPath = item.dataset.path;
                if (itemPath !== this.state.currentPath) {
                    this.state.pathHistory = [itemPath];
                    this.loadFiles(itemPath);
                }
            });
        });
    }

    async loadProcesses() {
        const processList = document.getElementById('process-list');
        if (!processList) return;

        processList.innerHTML = '<div class="loading">加载中...</div>';

        try {
            const processes = await XiaoAPI.processList();
            this.state.processes = processes;
            this.renderProcessList(processes);
        } catch (error) {
            console.error('Failed to load processes:', error);
            processList.innerHTML = '<div class="empty-message"><span>⚙️</span><p>无法加载进程列表</p></div>';
            this.showToast('加载进程失败', 'error');
        }
    }

    renderProcessList(processes) {
        const processList = document.getElementById('process-list');
        if (!processList) return;

        processList.innerHTML = processes.map(proc => `
            <div class="process-item" data-pid="${proc.pid}">
                <span class="col-pid">${proc.pid}</span>
                <span class="col-pname">${proc.name}</span>
                <span class="col-pcpu ${proc.cpu > 50 ? 'high' : ''}">${proc.cpu.toFixed(1)}%</span>
                <span class="col-pmem ${proc.memory > 50 ? 'high' : ''}">${proc.memory.toFixed(1)}%</span>
                <span class="col-pstatus ${proc.status}">${proc.status}</span>
                <span class="col-puser">${proc.user}</span>
            </div>
        `).join('');

        processList.querySelectorAll('.process-item').forEach(item => {
            item.addEventListener('click', () => this.handleProcessClick(item));
        });
    }

    handleProcessClick(item) {
        document.querySelectorAll('.process-item').forEach(el => el.classList.remove('selected'));
        item.classList.add('selected');

        const pid = parseInt(item.dataset.pid);
        this.state.selectedProcess = pid;

        document.getElementById('kill-selected').disabled = false;
    }

    async refreshProcesses() {
        await this.loadProcesses();
        this.showToast('进程列表已刷新', 'success');
    }

    async killSelectedProcess() {
        if (!this.state.selectedProcess) return;

        try {
            const result = await XiaoAPI.processKill(this.state.selectedProcess);
            if (result.success) {
                this.showToast(`进程 ${this.state.selectedProcess} 已终止`, 'success');
                this.state.selectedProcess = null;
                document.getElementById('kill-selected').disabled = true;
                await this.loadProcesses();
            } else {
                this.showToast(`无法终止进程 ${this.state.selectedProcess}`, 'error');
            }
        } catch (error) {
            console.error('Failed to kill process:', error);
            this.showToast('终止进程失败', 'error');
        }
    }

    updateProcessStats() {
        document.getElementById('total-processes').textContent = this.state.processes.length;
        
        const running = this.state.processes.filter(p => p.status === 'running').length;
        document.getElementById('running-processes').textContent = running;

        const totalCpu = this.state.processes.reduce((sum, p) => sum + p.cpu, 0);
        const totalMem = this.state.processes.reduce((sum, p) => sum + p.memory, 0);

        document.getElementById('process-cpu').textContent = `${totalCpu.toFixed(1)}%`;
        document.getElementById('process-memory').textContent = `${totalMem.toFixed(1)}%`;
    }

    formatUptime(seconds) {
        const days = Math.floor(seconds / 86400);
        const hours = Math.floor((seconds % 86400) / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);

        if (days > 0) {
            return `${days}天 ${hours}小时`;
        } else if (hours > 0) {
            return `${hours}小时 ${minutes}分钟`;
        } else {
            return `${minutes}分钟`;
        }
    }

    formatBytes(bytes) {
        if (bytes === 0) return '0 B';
        const k = 1024;
        const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }

    formatSpeed(bytesPerSec) {
        if (bytesPerSec < 1024) return bytesPerSec.toFixed(0) + ' B/s';
        if (bytesPerSec < 1024 * 1024) return (bytesPerSec / 1024).toFixed(1) + ' KB/s';
        return (bytesPerSec / 1024 / 1024).toFixed(1) + ' MB/s';
    }

    formatDate(timestamp) {
        if (!timestamp) return '-';
        const date = new Date(timestamp);
        return date.toLocaleString('zh-CN', {
            year: 'numeric',
            month: '2-digit',
            day: '2-digit',
            hour: '2-digit',
            minute: '2-digit'
        });
    }

    showToast(message, type = 'info') {
        const container = document.getElementById('toast-container');
        if (!container) return;

        const icons = {
            success: '✅',
            error: '❌',
            warning: '⚠️',
            info: 'ℹ️'
        };

        const toast = document.createElement('div');
        toast.className = `toast ${type}`;
        toast.innerHTML = `
            <span class="toast-icon">${icons[type]}</span>
            <span class="toast-message">${message}</span>
            <button class="toast-close">&times;</button>
        `;

        toast.querySelector('.toast-close').addEventListener('click', () => {
            toast.remove();
        });

        container.appendChild(toast);

        setTimeout(() => {
            if (toast.parentNode) {
                toast.remove();
            }
        }, 4000);
    }
}

document.addEventListener('DOMContentLoaded', () => {
    window.xiaoOS = new XiaoKangOS();
});
