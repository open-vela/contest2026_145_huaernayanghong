import React, { useState, useEffect, useRef } from 'react';
import { Header } from './components/Header.jsx';
import { NavTabs } from './components/NavTabs.jsx';
import { ToolExecModal } from './components/ToolExecModal.jsx';
import { Overview } from './views/overview/index.jsx';
import { Agent } from './views/agent/index.jsx';
import { Settings } from './views/settings/index.jsx';
import { Storage } from './views/storage/index.jsx';
import { Terminal } from './views/terminal/index.jsx';
import { Audio } from './views/audio/index.jsx';
import { systemApi } from './api/system.js';

export function App() {
  const [activeTab, setActiveTab] = useState('overview');
  const [statusData, setStatusData] = useState(null);
  const [modalTool, setModalTool] = useState(null);

  // 轮询定时器与避让标志
  const statusTimerRef = useRef(null);
  const isPollingPausedRef = useRef(false);

  const fetchStatus = async () => {
    if (isPollingPausedRef.current) return;
    try {
      const d = await systemApi.getStatus();
      if (d) setStatusData(d);
    } catch (e) {}
  };

  useEffect(() => {
    fetchStatus();
    statusTimerRef.current = setInterval(fetchStatus, 2000);

    // 同步客户端时间
    systemApi.syncTime(Math.floor(Date.now() / 1000)).catch(() => {});

    // 页面可见性挂起
    const handleVisibilityChange = () => {
      if (document.hidden) {
        if (statusTimerRef.current) clearInterval(statusTimerRef.current);
        statusTimerRef.current = setInterval(fetchStatus, 5000);
      } else {
        if (statusTimerRef.current) clearInterval(statusTimerRef.current);
        statusTimerRef.current = setInterval(fetchStatus, 2000);
        fetchStatus();
      }
    };

    document.addEventListener('visibilitychange', handleVisibilityChange);
    return () => {
      if (statusTimerRef.current) clearInterval(statusTimerRef.current);
      document.removeEventListener('visibilitychange', handleVisibilityChange);
    };
  }, []);

  const pausePolling = () => {
    isPollingPausedRef.current = true;
    if (statusTimerRef.current) {
      clearInterval(statusTimerRef.current);
      statusTimerRef.current = null;
    }
  };

  const resumePolling = () => {
    isPollingPausedRef.current = false;
    if (!statusTimerRef.current) {
      statusTimerRef.current = setInterval(fetchStatus, 2000);
    }
  };

  return (
    <>
      <Header network={statusData?.network} />
      <NavTabs activeTab={activeTab} onChangeTab={setActiveTab} />

      <main className="main-wrap">
        <div className={`view-panel ${activeTab === 'overview' ? 'active' : ''}`}>
          <Overview statusData={statusData} onRefreshStatus={fetchStatus} />
        </div>

        <div className={`view-panel ${activeTab === 'agent' ? 'active' : ''}`}>
          <Agent
            onOpenToolModal={(t) => setModalTool(t)}
            pausePolling={pausePolling}
            resumePolling={resumePolling}
          />
        </div>

        <div className={`view-panel ${activeTab === 'settings' ? 'active' : ''}`}>
          <Settings />
        </div>

        <div className={`view-panel ${activeTab === 'storage' ? 'active' : ''}`}>
          <Storage storageInfo={statusData?.storage} />
        </div>

        <div className={`view-panel ${activeTab === 'terminal' ? 'active' : ''}`}>
          <Terminal isActive={activeTab === 'terminal'} />
        </div>

        <div className={`view-panel ${activeTab === 'audio' ? 'active' : ''}`}>
          <Audio isActive={activeTab === 'audio'} />
        </div>
      </main>

      {/* 单步具身驱动直调模态框 */}
      <ToolExecModal
        isOpen={modalTool !== null}
        tool={modalTool}
        onClose={() => setModalTool(null)}
        onSuccess={fetchStatus}
      />

      {/* Toast 提示容器 */}
      <div className="toast" id="toastMsg"></div>
    </>
  );
}
