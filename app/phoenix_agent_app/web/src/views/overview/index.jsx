import React, { useState, useEffect } from 'react';
import { systemApi } from '../../api/system.js';
import { showToast } from '../../utils/toast.js';

export function Overview({ statusData, onRefreshStatus }) {
  // 本地设备硬件滑块
  const [vol, setVol] = useState(80);
  const [bright, setBright] = useState(90);

  // 备忘录输入
  const [memoText, setMemoText] = useState('');

  // 天气状态
  const [weather, setWeather] = useState({
    city: '上海',
    temp_c: '--',
    condition: '正在获取',
    temp_min: '--',
    temp_max: '--',
    humidity: '--',
    is_valid: false,
    is_fetching: false,
    update_time: 0
  });
  const [weatherCityInput, setWeatherCityInput] = useState('上海');

  // 待办事项状态
  const [todos, setTodos] = useState([]);
  const [doneCount, setDoneCount] = useState(0);
  const [totalCount, setTotalCount] = useState(0);
  const [todoTitle, setTodoTitle] = useState('');
  const [todoTime, setTodoTime] = useState('');

  // 加载天气与待办
  const loadWeather = async () => {
    try {
      const d = await systemApi.getWeatherStatus();
      if (d && d.success) {
        setWeather(d);
        if (d.city) setWeatherCityInput(d.city);
      }
    } catch (e) {}
  };

  const loadTodos = async () => {
    try {
      const d = await systemApi.getTodoList();
      if (d && d.success) {
        setTodos(d.todos || []);
        setDoneCount(d.done_count || 0);
        setTotalCount(d.total_count || 0);
      }
    } catch (e) {}
  };

  useEffect(() => {
    loadWeather();
    loadTodos();
  }, []);

  // 硬件控制同步
  const syncHardwareCtrl = (newVol, newBright) => {
    systemApi.setHardwareConfig(newVol, newBright).catch(() => {});
  };

  const handleVolChange = (v) => {
    const val = parseInt(v);
    setVol(val);
    syncHardwareCtrl(val, bright);
  };

  const handleBrightChange = (b) => {
    const val = parseInt(b);
    setBright(val);
    syncHardwareCtrl(vol, val);
  };

  // 卡带动作
  const handleCartridgeSwitch = async (id) => {
    try {
      await systemApi.switchCartridge(id);
      showToast('📺 开发板真机屏幕已切换至: ' + id);
      if (onRefreshStatus) onRefreshStatus();
    } catch (e) {
      showToast('卡带切换失败');
    }
  };

  const handleCartridgeAction = async (act) => {
    try {
      await systemApi.doCartridgeAction(act);
      if (onRefreshStatus) onRefreshStatus();
    } catch (e) {}
  };

  // 灵感外脑
  const handleAddMemo = async () => {
    if (!memoText.trim()) return;
    try {
      await systemApi.addMemo(memoText.trim());
      setMemoText('');
      showToast('💡 灵感已推入端侧外脑！');
      if (onRefreshStatus) onRefreshStatus();
    } catch (e) {
      showToast('保存灵感失败');
    }
  };

  // 天气操作
  const handleSaveWeatherCity = async (cityToSave) => {
    const c = cityToSave || weatherCityInput.trim();
    if (!c) { showToast('请输入城市名'); return; }
    try {
      await systemApi.setWeatherConfig(c);
      showToast('🌤️ 城市已变更为 [' + c + '], 硬件屏幕正同步拉取');
      loadWeather();
    } catch (e) {
      showToast('设置失败: ' + e.message);
    }
  };

  // 待办操作
  const handleToggleTodo = async (id) => {
    try {
      await systemApi.toggleTodo(id);
      showToast('📝 待办状态已更新');
      loadTodos();
    } catch (e) {
      showToast('更新待办失败');
    }
  };

  const handleAddTodo = async () => {
    if (!todoTitle.trim()) { showToast('请输入待办事项内容'); return; }
    try {
      const d = await systemApi.addTodo(todoTitle.trim(), todoTime.trim() || '今日');
      if (d.success) {
        setTodoTitle('');
        setTodoTime('');
        showToast('➕ 待办添加成功，已推送到真机桌面');
        loadTodos();
      } else {
        showToast('添加失败: ' + (d.error || ''));
      }
    } catch (e) {
      showToast('添加失败');
    }
  };

  const handleDeleteTodo = async (id) => {
    try {
      await systemApi.deleteTodo(id);
      showToast('🗑️ 待办已删除');
      loadTodos();
    } catch (e) {
      showToast('删除失败');
    }
  };

  const handleClearDoneTodos = async () => {
    try {
      const d = await systemApi.clearDoneTodos();
      showToast(`🧹 已清理 ${d.cleared_count || 0} 条已完成待办`);
      loadTodos();
    } catch (e) {
      showToast('清理失败');
    }
  };

  // 提取遥测与卡带数据
  const t = statusData?.telemetry || {};
  const c = statusData?.cartridges || {};
  const s = statusData?.stats || {};
  const activeCartridge = statusData?.active_cartridge || 'familiar';

  const cartridgeNames = { familiar: '使魔萌宠', memo: '灵感外脑', clock: '番茄时钟', zen: '赛博木鱼', home: '主页', agent: '灵眸AI' };
  const upSec = t.uptime || 0;
  const h = Math.floor(upSec / 3600);
  const m = Math.floor((upSec % 3600) / 60);
  const sec = upSec % 60;
  const uptimeStr = `${h}h ${m}m ${sec}s`;

  const pomoRemainSec = c.clock?.remain_s || 0;
  const pomoMm = String(Math.floor(pomoRemainSec / 60)).padStart(2, '0');
  const pomoSs = String(pomoRemainSec % 60).padStart(2, '0');
  const pomoRemainStr = c.clock?.is_pomodoro ? `${pomoMm}:${pomoSs}` : '25:00';

  return (
    <div>
      {/* 硬件遥测看板 */}
      <div className="card highlight">
        <div className="card-head">
          <div className="card-title">🩺 全志 R528-S3 硬件遥测与系统负载</div>
          <span className="status-badge">{t.fps || 60} FPS</span>
        </div>
        <div className="grid-4">
          <div className="m-block">
            <div className="m-lbl">CPU 负荷 & 频率</div>
            <div className="m-val">{t.cpu_load_pct || 0} %</div>
            <div style={{ fontSize: '10px', color: 'var(--muted)', marginTop: '2px' }}>{t.cpu_freq_mhz || 1008} MHz</div>
          </div>
          <div className="m-block">
            <div className="m-lbl">内存使用率 (RAM)</div>
            <div className="m-val">{t.mem_used_pct || 0} %</div>
            <div style={{ fontSize: '10px', color: 'var(--muted)', marginTop: '2px' }}>Free: {Math.round((t.mem_free_kb || 0) / 1024)} MB</div>
          </div>
          <div className="m-block">
            <div className="m-lbl">芯片温度 (Core Temp)</div>
            <div className="m-val">{t.temp_c || 42} °C</div>
            <div style={{ fontSize: '10px', color: 'var(--muted)', marginTop: '2px' }}>健康安全</div>
          </div>
          <div className="m-block">
            <div className="m-lbl">系统运行时长 (Uptime)</div>
            <div className="m-val" style={{ fontSize: '15px', lineHeight: '22px' }}>{uptimeStr}</div>
            <div style={{ fontSize: '10px', color: 'var(--muted)', marginTop: '2px' }}>{t.os || 'RTOS / Linux'}</div>
          </div>
        </div>
        {/* 快捷设备控制 (音量/背光) */}
        <div style={{
          display: 'flex',
          flexWrap: 'wrap',
          gap: '16px',
          background: 'var(--card-inner)',
          padding: '12px 16px',
          borderRadius: '8px',
          border: '1px solid var(--border-light)',
          alignItems: 'center'
        }}>
          <div style={{ flex: 1, minWidth: '180px', display: 'flex', alignItems: 'center', gap: '8px', fontSize: '12px' }}>
            <span>🔊 音量:</span>
            <input
              type="range"
              min="0"
              max="100"
              value={vol}
              onChange={(e) => handleVolChange(e.target.value)}
              style={{ flex: 1 }}
            />
            <span style={{ minWidth: '36px', textAlign: 'right', fontWeight: 'bold', color: 'var(--accent)' }}>{vol}%</span>
          </div>
          <div style={{ flex: 1, minWidth: '180px', display: 'flex', alignItems: 'center', gap: '8px', fontSize: '12px' }}>
            <span>☀️ 屏幕背光:</span>
            <input
              type="range"
              min="10"
              max="100"
              value={bright}
              onChange={(e) => handleBrightChange(e.target.value)}
              style={{ flex: 1 }}
            />
            <span style={{ minWidth: '36px', textAlign: 'right', fontWeight: 'bold', color: 'var(--accent)' }}>{bright}%</span>
          </div>
          <button
            className="btn-secondary"
            style={{ margin: 0, padding: '6px 12px', fontSize: '11px' }}
            onClick={async () => {
              await systemApi.triggerProactive();
              showToast('⚡ 已向端侧触发主动式交互演练！');
            }}
          >
            ⚡ 演练主动交互
          </button>
        </div>
      </div>

      {/* 4 大伴侣卡带联动控制台 */}
      <div style={{ fontSize: '13px', fontWeight: 'bold', color: 'var(--muted)', marginBottom: '10px', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <span>🎮 桌面具身数字生命体卡带 (Cartridges)</span>
        <span style={{ fontSize: '11px', color: 'var(--accent)' }}>
          当前真机屏幕: {cartridgeNames[activeCartridge] || activeCartridge}
        </span>
      </div>

      {/* 真机屏幕卡带即时切换药丸栏 */}
      <div style={{ display: 'flex', gap: '8px', marginBottom: '16px', overflowX: 'auto', paddingBottom: '4px' }}>
        {[
          { id: 'home', label: '🏠 极客主页' },
          { id: 'familiar', label: '🐱 桌面使魔' },
          { id: 'memo', label: '💡 灵感外脑' },
          { id: 'clock', label: '🍅 番茄时钟' },
          { id: 'zen', label: '🪷 赛博木鱼' },
          { id: 'agent', label: '🤖 灵眸AI' },
        ].map(item => (
          <button
            key={item.id}
            className={`chip ${activeCartridge === item.id ? 'active' : ''}`}
            style={{ padding: '7px 14px', fontSize: '12px' }}
            onClick={() => handleCartridgeSwitch(item.id)}
          >
            {item.label}
          </button>
        ))}
      </div>

      <div className="grid-2">
        {/* 使魔 */}
        <div className="card">
          <div className="card-head">
            <div className="card-title">🐱 桌面使魔萌宠</div>
            <div style={{ display: 'flex', gap: '6px', alignItems: 'center' }}>
              <span className="status-badge">陪伴中</span>
              <button className="btn-secondary" style={{ padding: '3px 8px', fontSize: '11px' }} onClick={() => handleCartridgeSwitch('familiar')}>
                📺 投到屏幕
              </button>
            </div>
          </div>
          <div className="metric-row">
            <div className="m-block"><div className="m-lbl">使魔亲密度</div><div className="m-val">{c.familiar?.affinity ?? 88}</div></div>
            <div className="m-block"><div className="m-lbl">饱腹元气度</div><div className="m-val">96%</div></div>
          </div>
          <button className="btn-secondary" style={{ width: '100%' }} onClick={() => handleCartridgeAction('pet')}>
            🐾 远程轻抚使魔撒娇
          </button>
        </div>

        {/* 灵感外脑 */}
        <div className="card">
          <div className="card-head">
            <div className="card-title">💡 灵感外脑速记</div>
            <div style={{ display: 'flex', gap: '6px', alignItems: 'center' }}>
              <span className="status-badge">{c.memo?.count ?? 0} 条</span>
              <button className="btn-secondary" style={{ padding: '3px 8px', fontSize: '11px' }} onClick={() => handleCartridgeSwitch('memo')}>
                📺 投到屏幕
              </button>
            </div>
          </div>
          <div style={{
            background: 'var(--card-inner)',
            padding: '8px 12px',
            borderRadius: '6px',
            fontSize: '12px',
            marginBottom: '8px',
            borderLeft: '2px solid var(--accent)',
            color: '#c0d2e8'
          }}>
            {c.memo?.last_memo || '正在同步端侧备忘...'}
          </div>
          <div style={{ display: 'flex', gap: '8px' }}>
            <input
              type="text"
              placeholder="输入闪念即刻存入外脑..."
              value={memoText}
              onChange={(e) => setMemoText(e.target.value)}
              onKeyDown={(e) => e.key === 'Enter' && handleAddMemo()}
            />
            <button style={{ whiteSpace: 'nowrap', padding: '8px 14px' }} onClick={handleAddMemo}>
              💡 存入
            </button>
          </div>
        </div>

        {/* 番茄专注时钟 */}
        <div className="card">
          <div className="card-head">
            <div className="card-title">🍅 番茄专注时钟</div>
            <div style={{ display: 'flex', gap: '6px', alignItems: 'center' }}>
              <span className="status-badge">{c.clock?.is_pomodoro ? '🍅 专注中' : '待命'}</span>
              <button className="btn-secondary" style={{ padding: '3px 8px', fontSize: '11px' }} onClick={() => handleCartridgeSwitch('clock')}>
                📺 投到屏幕
              </button>
            </div>
          </div>
          <div className="metric-row">
            <div className="m-block"><div className="m-lbl">专注倒计时</div><div className="m-val">{pomoRemainStr}</div></div>
            <div className="m-block"><div className="m-lbl">历史专注轮次</div><div className="m-val">3 轮</div></div>
          </div>
          <button className="btn-secondary" style={{ width: '100%' }} onClick={() => handleCartridgeAction('pomo_toggle')}>
            🍅 开启 / 暂停 / 继续专注
          </button>
        </div>

        {/* 赛博木鱼 */}
        <div className="card">
          <div className="card-head">
            <div className="card-title">🪷 极客禅意木鱼</div>
            <div style={{ display: 'flex', gap: '6px', alignItems: 'center' }}>
              <span className="status-badge" style={{ color: 'var(--accent-sub)' }}>消除焦虑</span>
              <button className="btn-secondary" style={{ padding: '3px 8px', fontSize: '11px' }} onClick={() => handleCartridgeSwitch('zen')}>
                📺 投到屏幕
              </button>
            </div>
          </div>
          <div className="metric-row">
            <div className="m-block"><div className="m-lbl">累计功德数</div><div className="m-val">{c.zen?.total_merit || s.merit || 0}</div></div>
            <div className="m-block"><div className="m-lbl">LLM 消耗 Tokens</div><div className="m-val">{s.total_tokens || 0}</div></div>
          </div>
          <button style={{ width: '100%' }} onClick={() => handleCartridgeAction('knock_fish')}>
            🪷 敲击木鱼 (功德+1)
          </button>
        </div>

        {/* 桌面极客天气卡片 */}
        <div className="card">
          <div className="card-head">
            <div className="card-title">🌤️ 桌面极客天气仪表盘</div>
            <div style={{ display: 'flex', gap: '6px', alignItems: 'center' }}>
              <span className="status-badge" style={{ color: weather.is_fetching ? 'var(--accent-warn)' : 'var(--accent)' }}>
                {weather.is_fetching ? '同步中...' : (weather.is_valid ? '实时同步' : '未连接')}
              </span>
              <button className="btn-secondary" style={{ padding: '3px 8px', fontSize: '11px' }} onClick={() => handleSaveWeatherCity()}>
                🔄 立即拉取
              </button>
            </div>
          </div>
          <div className="metric-row">
            <div className="m-block">
              <div className="m-lbl">当前气温 & 状况</div>
              <div className="m-val" style={{ color: 'var(--text)', fontSize: '22px' }}>
                {weather.is_valid ? `${weather.temp_c}°C` : '--°C'}
              </div>
              <div style={{ fontSize: '11px', color: 'var(--accent)' }}>
                {weather.city} · {weather.condition}
              </div>
            </div>
            <div className="m-block">
              <div className="m-lbl">气温区间 & 相对湿度</div>
              <div className="m-val" style={{ fontSize: '16px', color: 'var(--accent-sub)' }}>
                {weather.is_valid ? `${weather.temp_min}° ~ ${weather.temp_max}°C` : '-- ~ --°C'}
              </div>
              <div style={{ fontSize: '11px', color: 'var(--muted)' }}>
                相对湿度: {weather.is_valid ? `${weather.humidity}%` : '--%'}
              </div>
            </div>
          </div>
          {/* 城市配置与快捷选择 */}
          <div style={{
            background: 'var(--card-inner)',
            padding: '10px 12px',
            borderRadius: '8px',
            border: '1px solid var(--border-light)',
            marginTop: '8px'
          }}>
            <div style={{ fontSize: '11px', color: 'var(--muted)', marginBottom: '6px', display: 'flex', justifyContent: 'space-between' }}>
              <span>城市配置 (更改后硬件屏幕即时同步)</span>
              <span style={{ color: '#5f7899' }}>
                {weather.update_time > 0 ? '更新于 ' + new Date(weather.update_time * 1000).toLocaleTimeString() : '-'}
              </span>
            </div>
            <div style={{ display: 'flex', gap: '8px', marginBottom: '8px' }}>
              <input
                type="text"
                placeholder="输入城市 (如 上海 / 北京 / 深圳)"
                value={weatherCityInput}
                onChange={(e) => setWeatherCityInput(e.target.value)}
                style={{ flex: 1 }}
              />
              <button
                className="btn-primary"
                style={{ whiteSpace: 'nowrap', padding: '6px 14px', fontSize: '12px' }}
                onClick={() => handleSaveWeatherCity()}
              >
                💾 保存并同步
              </button>
            </div>
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
              {['上海', '北京', '深圳', '广州', '杭州', '成都'].map(cityName => (
                <span
                  key={cityName}
                  className="chip"
                  style={{ padding: '3px 8px', fontSize: '11px' }}
                  onClick={() => {
                    setWeatherCityInput(cityName);
                    handleSaveWeatherCity(cityName);
                  }}
                >
                  {cityName}
                </span>
              ))}
            </div>
          </div>
        </div>

        {/* 今日待办事项管理看板 */}
        <div className="card">
          <div className="card-head">
            <div className="card-title">📝 今日待办事项看板 (Todo List)</div>
            <div style={{ display: 'flex', gap: '6px', alignItems: 'center' }}>
              <span className="status-badge" style={{ color: 'var(--accent-sub)' }}>
                {doneCount}/{totalCount} 达成
              </span>
              <button className="btn-secondary" style={{ padding: '3px 8px', fontSize: '11px' }} onClick={handleClearDoneTodos}>
                🧹 清理已完成
              </button>
            </div>
          </div>
          {/* 动态待办列表 */}
          <div style={{ display: 'flex', flexDirection: 'column', gap: '6px', maxHeight: '180px', overflowY: 'auto', marginBottom: '10px', paddingRight: '2px' }}>
            {todos.length === 0 ? (
              <div style={{ fontSize: '12px', color: 'var(--muted)', textAlign: 'center', padding: '16px' }}>
                暂无待办事项，在下方输入框添加吧！
              </div>
            ) : (
              todos.map(it => (
                <div
                  key={it.id}
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                    background: 'var(--card-inner)',
                    padding: '8px 12px',
                    borderRadius: '6px',
                    border: `1px solid ${it.done ? 'rgba(255,255,255,0.04)' : '#1e3352'}`,
                    transition: 'all 0.2s'
                  }}
                >
                  <input
                    type="checkbox"
                    checked={!!it.done}
                    onChange={() => handleToggleTodo(it.id)}
                    style={{ cursor: 'pointer', width: '16px', height: '16px' }}
                  />
                  <span style={{
                    flex: 1,
                    fontSize: '13px',
                    color: it.done ? 'var(--muted)' : 'var(--text)',
                    textDecoration: it.done ? 'line-through' : 'none',
                    fontWeight: it.done ? 'normal' : 600
                  }}>
                    {it.title}
                  </span>
                  <span style={{
                    fontSize: '11px',
                    color: it.done ? 'var(--muted)' : 'var(--accent-warn)',
                    background: '#141f33',
                    padding: '2px 6px',
                    borderRadius: '4px'
                  }}>
                    {it.time || '今日'}
                  </span>
                  <button
                    className="btn-secondary"
                    style={{ padding: '2px 6px', fontSize: '11px', color: '#ff5555', borderColor: 'rgba(255,85,85,0.2)' }}
                    onClick={() => handleDeleteTodo(it.id)}
                  >
                    ✕
                  </button>
                </div>
              ))
            )}
          </div>
          {/* 快速添加新待办 */}
          <div style={{ display: 'flex', gap: '6px', background: 'var(--card-inner)', padding: '8px', borderRadius: '8px', border: '1px solid var(--border-light)' }}>
            <input
              type="text"
              placeholder="新增待办任务..."
              style={{ flex: 1 }}
              value={todoTitle}
              onChange={(e) => setTodoTitle(e.target.value)}
              onKeyDown={(e) => e.key === 'Enter' && handleAddTodo()}
            />
            <input
              type="text"
              placeholder="时间 (如 16:00)"
              style={{ width: '100px' }}
              value={todoTime}
              onChange={(e) => setTodoTime(e.target.value)}
              onKeyDown={(e) => e.key === 'Enter' && handleAddTodo()}
            />
            <button
              className="btn-primary"
              style={{ whiteSpace: 'nowrap', padding: '6px 14px', fontSize: '12px' }}
              onClick={handleAddTodo}
            >
              ➕ 添加
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
