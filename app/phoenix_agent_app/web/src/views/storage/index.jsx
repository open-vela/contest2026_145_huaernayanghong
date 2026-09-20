import React, { useState, useEffect, useRef } from 'react';
import { storageApi } from '../../api/storage.js';
import { showToast } from '../../utils/toast.js';
import { formatBytes } from '../../utils/helpers.js';

export function Storage({ storageInfo }) {
  const [curPath, setCurPath] = useState('/');
  const [items, setItems] = useState([]);
  const [loading, setLoading] = useState(false);
  const [newDirName, setNewDirName] = useState('');
  const [uploadFileName, setUploadFileName] = useState('');
  const [uploadFileContent, setUploadFileContent] = useState('');

  const fileInputRef = useRef(null);

  const loadList = async (pathToLoad) => {
    const p = pathToLoad !== undefined ? pathToLoad : curPath;
    setLoading(true);
    try {
      const d = await storageApi.getList(p);
      setLoading(false);
      if (d && d.success) {
        const sorted = (d.items || []).sort((a, b) => (b.is_dir - a.is_dir) || a.name.localeCompare(b.name));
        setItems(sorted);
      } else {
        setItems([]);
      }
    } catch (e) {
      setLoading(false);
      setItems([]);
    }
  };

  useEffect(() => {
    loadList(curPath);
  }, [curPath]);

  const handleGoPath = (p) => {
    const target = p.startsWith('/') ? p : '/' + p;
    setCurPath(target);
  };

  const handleGoUp = () => {
    if (curPath === '/' || !curPath) return;
    const parts = curPath.split('/').filter(Boolean);
    parts.pop();
    setCurPath('/' + parts.join('/'));
  };

  const handleMkdir = async () => {
    if (!newDirName.trim()) return;
    const target = (curPath === '/' ? '' : curPath) + '/' + newDirName.trim();
    try {
      const d = await storageApi.mkdir(target);
      if (d.success) {
        setNewDirName('');
        loadList();
      } else {
        alert('创建失败: ' + (d.error || '权限或路径错误'));
      }
    } catch (e) {
      alert('创建失败: ' + e.message);
    }
  };

  const handleUploadText = async () => {
    if (!uploadFileName.trim()) { alert('请输入目标文件名'); return; }
    const target = (curPath === '/' ? '' : curPath) + '/' + uploadFileName.trim();
    try {
      const d = await storageApi.uploadText(target, uploadFileContent);
      if (d.success) {
        showToast('✅ 文件已写入 TF 卡');
        setUploadFileName('');
        setUploadFileContent('');
        loadList();
      } else {
        alert('保存失败: ' + (d.error || '未知错误'));
      }
    } catch (e) {
      alert('保存失败: ' + e.message);
    }
  };

  const handleLocalFileChange = (e) => {
    if (e.target.files && e.target.files[0]) {
      const file = e.target.files[0];
      setUploadFileName(file.name);
      const reader = new FileReader();
      reader.onload = function(evt) {
        setUploadFileContent(evt.target.result);
      };
      reader.readAsText(file);
    }
  };

  const handleDelete = async (targetPath, isDir) => {
    if (!confirm(`确认永久删除该${isDir ? '目录' : '文件'}：\n${targetPath} ？`)) return;
    try {
      const d = await storageApi.deleteItem(targetPath);
      if (d.success) loadList();
      else alert('删除失败: ' + (d.error || '非空或权限不足'));
    } catch (e) {
      alert('删除失败: ' + e.message);
    }
  };

  const st = storageInfo || {};
  const isMounted = !!st.sdcard_mounted;
  const usedPct = st.sdcard_used_pct || 0;
  const totalMb = st.sdcard_total_mb || 0;
  const freeMb = st.sdcard_free_mb || 0;
  const usedMb = totalMb >= freeMb ? (totalMb - freeMb) : 0;

  return (
    <div className="card highlight">
      <div className="card-head">
        <div className="card-title">📁 外置 MicroSD / TF 卡存储系统</div>
        <span className="status-badge" style={{ color: isMounted ? 'var(--accent-sub)' : 'var(--muted)' }}>
          {isMounted ? '● 已挂载就绪' : '○ 未检测到 TF 卡'}
        </span>
      </div>

      {/* 容量进度条 */}
      <div style={{ background: 'var(--card-inner)', border: '1px solid var(--border)', borderRadius: '10px', padding: '12px', marginBottom: '16px' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '12px', marginBottom: '8px' }}>
          <span style={{ color: 'var(--muted)' }}>挂载目录: <b style={{ color: 'var(--accent)' }}>{st.sdcard_mount_point || '/mnt/sdcard'}</b></span>
          <span style={{ color: 'var(--muted)' }}>已用: <b style={{ color: 'var(--accent-sub)' }}>{usedPct}%</b> ({usedMb} MB / {totalMb} MB)</span>
        </div>
        <div style={{ height: '8px', background: '#141c2c', borderRadius: '4px', overflow: 'hidden' }}>
          <div style={{
            width: `${Math.min(100, Math.max(0, usedPct))}%`,
            height: '100%',
            background: 'linear-gradient(90deg, var(--accent), var(--accent-sub))',
            transition: 'width 0.4s ease'
          }} />
        </div>
        <div style={{ display: 'flex', gap: '12px', marginTop: '10px', fontSize: '11px', color: 'var(--muted)' }}>
          <span>📌 持久配置: <code style={{ color: '#9bc0e7' }}>{st.data_dir || '/data/phoenix'}</code></span>
          <span>⚡ 中间分片: <code style={{ color: '#9bc0e7' }}>{st.temp_dir || '/tmp/phoenix'}</code></span>
        </div>
      </div>

      {/* 目录导航 */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', flexWrap: 'wrap', gap: '8px', marginBottom: '12px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <span style={{ fontSize: '12px', color: 'var(--muted)' }}>当前路径:</span>
          <code style={{ background: '#182842', color: 'var(--accent)', padding: '4px 8px', borderRadius: '6px', fontSize: '12px', fontWeight: 'bold' }}>
            {curPath}
          </code>
          <button className="btn-secondary" style={{ margin: 0, padding: '4px 10px', fontSize: '11px' }} onClick={handleGoUp}>
            ⬆️ 上一级
          </button>
          <button className="btn-secondary" style={{ margin: 0, padding: '4px 10px', fontSize: '11px' }} onClick={() => loadList()}>
            🔄 刷新
          </button>
        </div>
        <div style={{ display: 'flex', gap: '6px' }}>
          {['/sounds', '/logs', '/docs'].map(p => (
            <button key={p} className="btn-secondary" style={{ margin: 0, padding: '4px 10px', fontSize: '11px' }} onClick={() => handleGoPath(p)}>
              {p === '/sounds' ? '🎵' : (p === '/logs' ? '📜' : '📑')} {p.substring(1)}
            </button>
          ))}
        </div>
      </div>

      {/* 文件表格 */}
      <div style={{ background: 'var(--card-inner)', border: '1px solid var(--border)', borderRadius: '8px', maxHeight: '280px', overflowY: 'auto', marginBottom: '16px' }}>
        <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '12px', textAlign: 'left' }}>
          <thead>
            <tr style={{ borderBottom: '1px solid var(--border)', color: 'var(--muted)' }}>
              <th style={{ padding: '8px 12px' }}>文件名称</th>
              <th style={{ padding: '8px 12px', width: '90px' }}>大小</th>
              <th style={{ padding: '8px 12px', width: '120px', textAlign: 'right' }}>操作</th>
            </tr>
          </thead>
          <tbody>
            {loading ? (
              <tr><td colSpan="3" style={{ padding: '16px', textAlign: 'center', color: 'var(--muted)' }}>正在读取 TF 卡文件系统...</td></tr>
            ) : items.length === 0 ? (
              <tr><td colSpan="3" style={{ padding: '16px', textAlign: 'center', color: 'var(--muted)' }}>当前目录为空</td></tr>
            ) : (
              items.map(it => {
                const fullItemPath = (curPath === '/' ? '' : curPath) + '/' + it.name;
                return (
                  <tr key={it.name} style={{ borderBottom: '1px solid rgba(255,255,255,0.04)' }}>
                    <td style={{ padding: '8px 12px' }}>
                      <span style={{ marginRight: '6px' }}>{it.is_dir ? '📁' : '📄'}</span>
                      {it.is_dir ? (
                        <span style={{ cursor: 'pointer', color: 'var(--accent)', fontWeight: 600 }} onClick={() => handleGoPath(fullItemPath)}>
                          {it.name}
                        </span>
                      ) : (
                        <span style={{ color: '#e6edf3' }}>{it.name}</span>
                      )}
                    </td>
                    <td style={{ padding: '8px 12px', color: 'var(--muted)' }}>
                      {it.is_dir ? <span style={{ color: 'var(--muted)' }}>[目录]</span> : formatBytes(it.size)}
                    </td>
                    <td style={{ padding: '8px 12px', textAlign: 'right' }}>
                      {!it.is_dir && (
                        <a
                          href={`/api/sdcard/download?path=${encodeURIComponent(fullItemPath)}`}
                          download={it.name}
                          style={{ color: 'var(--accent)', textDecoration: 'none', marginRight: '8px' }}
                        >
                          ⬇️下载
                        </a>
                      )}
                      <a
                        href="javascript:void(0)"
                        onClick={() => handleDelete(fullItemPath, it.is_dir)}
                        style={{ color: '#ff6b6b', textDecoration: 'none' }}
                      >
                        🗑️删除
                      </a>
                    </td>
                  </tr>
                );
              })
            )}
          </tbody>
        </table>
      </div>

      {/* 新建与上传 */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit,minmax(280px,1fr))', gap: '12px' }}>
        <div style={{ background: 'var(--card-inner)', padding: '12px', borderRadius: '8px', border: '1px solid var(--border-light)' }}>
          <div style={{ fontSize: '12px', color: 'var(--accent)', fontWeight: 600, marginBottom: '6px' }}>📁 新建子目录</div>
          <div style={{ display: 'flex', gap: '8px' }}>
            <input
              type="text"
              placeholder="目录名称 (如 music)"
              style={{ padding: '8px', fontSize: '12px' }}
              value={newDirName}
              onChange={(e) => setNewDirName(e.target.value)}
            />
            <button style={{ margin: 0, padding: '8px 14px', fontSize: '12px', whiteSpace: 'nowrap' }} onClick={handleMkdir}>
              新建
            </button>
          </div>
        </div>

        <div style={{ background: 'var(--card-inner)', padding: '12px', borderRadius: '8px', border: '1px solid var(--border-light)' }}>
          <div style={{ fontSize: '12px', color: 'var(--accent-sub)', fontWeight: 600, marginBottom: '6px' }}>✍️ 写入文本 / 上传文件</div>
          <input
            type="text"
            placeholder="目标文件名 (如 memo.txt)"
            style={{ padding: '8px', fontSize: '12px', marginBottom: '6px' }}
            value={uploadFileName}
            onChange={(e) => setUploadFileName(e.target.value)}
          />
          <textarea
            rows={2}
            placeholder="输入文件文本内容..."
            style={{ marginBottom: '6px', resize: 'none' }}
            value={uploadFileContent}
            onChange={(e) => setUploadFileContent(e.target.value)}
          />
          <div style={{ display: 'flex', gap: '8px' }}>
            <input type="file" ref={fileInputRef} style={{ display: 'none' }} onChange={handleLocalFileChange} />
            <button className="btn-secondary" style={{ margin: 0, padding: '6px 12px', fontSize: '11px' }} onClick={() => fileInputRef.current?.click()}>
              📂 选本地文件
            </button>
            <button style={{ margin: 0, padding: '6px 12px', fontSize: '11px', flex: 1 }} onClick={handleUploadText}>
              🚀 保存并写入 TF 卡
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
