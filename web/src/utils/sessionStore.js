/**
 * @file sessionStore.js
 * @brief Phoenix HoloDesk-S1 Web 伴侣本地会话持久化与生命周期管理
 * 基于 localStorage 实现零依赖结构化存储，支持多会话隔离、FIFO 消息容量保护与双端水合。
 */

const STORAGE_KEY_ACTIVE_SESSION = 'phoenix_active_session_id';
const STORAGE_KEY_SESSIONS_META = 'phoenix_sessions_meta';
const PREFIX_SESSION_MSGS = 'phoenix_session_msgs_';

const MAX_MESSAGES_PER_SESSION = 50; // 单会话最大保存消息数 (防止撑爆 localStorage)
const MAX_SESSIONS = 10;             // 最多保留历史会话数

/**
 * 安全解析 JSON，解析失败返回默认值
 */
function safeParse(str, fallback) {
  try {
    return str ? JSON.parse(str) : fallback;
  } catch (e) {
    return fallback;
  }
}

/**
 * 获取所有会话元信息列表 (按更新时间降序)
 * @returns {Array<{ id: string, title: string, createdAt: number, updatedAt: number, messageCount: number }>}
 */
export function listSessions() {
  const raw = localStorage.getItem(STORAGE_KEY_SESSIONS_META);
  const list = safeParse(raw, []);
  return Array.isArray(list) ? list.sort((a, b) => b.updatedAt - a.updatedAt) : [];
}

/**
 * 保存会话元信息列表
 */
function saveSessionsMeta(list) {
  try {
    localStorage.setItem(STORAGE_KEY_SESSIONS_META, JSON.stringify(list));
  } catch (e) {
    console.warn('[SessionStore] 保存会话元数据失败:', e);
  }
}

/**
 * 生成唯一 Session ID
 */
function generateSessionId() {
  return 'sess_' + Date.now().toString(36) + '_' + Math.random().toString(36).substring(2, 6);
}

/**
 * 获取当前活跃的 Session ID，如无则自动创建初始会话
 * @returns {string}
 */
export function getActiveSessionId() {
  let activeId = localStorage.getItem(STORAGE_KEY_ACTIVE_SESSION);
  const sessions = listSessions();

  if (activeId && sessions.some(s => s.id === activeId)) {
    return activeId;
  }

  if (sessions.length > 0) {
    activeId = sessions[0].id;
    localStorage.setItem(STORAGE_KEY_ACTIVE_SESSION, activeId);
    return activeId;
  }

  // 初始空态，新建默认会话
  const newSession = createNewSession('默认会话');
  return newSession.id;
}

/**
 * 创建新会话并自动切换为当前活跃会话
 * @param {string} [title='新话题']
 * @returns {{ id: string, title: string, createdAt: number, updatedAt: number, messageCount: number }}
 */
export function createNewSession(title = '新话题') {
  const sessions = listSessions();
  const now = Date.now();
  const newMeta = {
    id: generateSessionId(),
    title: title || '新话题',
    createdAt: now,
    updatedAt: now,
    messageCount: 0
  };

  // 会话数量超上限时淘汰最老的会话及其消息
  if (sessions.length >= MAX_SESSIONS) {
    const oldest = sessions.pop();
    if (oldest) {
      localStorage.removeItem(PREFIX_SESSION_MSGS + oldest.id);
    }
  }

  sessions.unshift(newMeta);
  saveSessionsMeta(sessions);
  localStorage.setItem(STORAGE_KEY_ACTIVE_SESSION, newMeta.id);
  localStorage.setItem(PREFIX_SESSION_MSGS + newMeta.id, JSON.stringify([]));

  return newMeta;
}

/**
 * 读取指定会话的消息列表
 * @param {string} sessionId
 * @returns {Array}
 */
export function loadSessionMessages(sessionId) {
  if (!sessionId) return [];
  const raw = localStorage.getItem(PREFIX_SESSION_MSGS + sessionId);
  return safeParse(raw, []);
}

/**
 * 保存指定会话的消息列表 (执行 FIFO 截断与元数据同步)
 * @param {string} sessionId
 * @param {Array} messages
 */
export function saveSessionMessages(sessionId, messages) {
  if (!sessionId || !Array.isArray(messages)) return;

  // 截取最新 MAX_MESSAGES_PER_SESSION 条，防止存储溢出
  const trimmed = messages.length > MAX_MESSAGES_PER_SESSION
    ? messages.slice(messages.length - MAX_MESSAGES_PER_SESSION)
    : messages;

  try {
    localStorage.setItem(PREFIX_SESSION_MSGS + sessionId, JSON.stringify(trimmed));
  } catch (e) {
    // 捕获 QuotaExceededError 异常，清理旧会话重试
    console.warn('[SessionStore] 存储超出配额，执行清理旧会话...');
    pruneOldSessions();
    try {
      localStorage.setItem(PREFIX_SESSION_MSGS + sessionId, JSON.stringify(trimmed));
    } catch (e2) {}
  }

  // 同步更新元数据
  const sessions = listSessions();
  const target = sessions.find(s => s.id === sessionId);
  if (target) {
    target.updatedAt = Date.now();
    target.messageCount = trimmed.length;

    // 若标题仍是默认名，尝试从首条用户消息中提取有意义的标题
    if (target.title === '默认会话' || target.title === '新话题') {
      const firstUserMsg = trimmed.find(m => m.role === 'user');
      if (firstUserMsg && firstUserMsg.content) {
        const clean = firstUserMsg.content.trim().replace(/\s+/g, ' ');
        target.title = clean.length > 14 ? clean.substring(0, 14) + '...' : clean;
      }
    }
    saveSessionsMeta(sessions);
  }
}

/**
 * 切换活跃会话
 * @param {string} sessionId
 */
export function switchActiveSession(sessionId) {
  localStorage.setItem(STORAGE_KEY_ACTIVE_SESSION, sessionId);
}

/**
 * 清空指定会话的消息列表 (保留会话元数据)
 * @param {string} sessionId
 */
export function clearSessionMessages(sessionId) {
  if (!sessionId) return;
  localStorage.setItem(PREFIX_SESSION_MSGS + sessionId, JSON.stringify([]));

  const sessions = listSessions();
  const target = sessions.find(s => s.id === sessionId);
  if (target) {
    target.messageCount = 0;
    target.updatedAt = Date.now();
    saveSessionsMeta(sessions);
  }
}

/**
 * 删除指定会话
 * @param {string} sessionId
 * @returns {string} 新的活跃会话 ID
 */
export function deleteSession(sessionId) {
  let sessions = listSessions().filter(s => s.id !== sessionId);
  localStorage.removeItem(PREFIX_SESSION_MSGS + sessionId);

  if (sessions.length === 0) {
    const fresh = createNewSession('默认会话');
    return fresh.id;
  }

  saveSessionsMeta(sessions);
  const currentActive = localStorage.getItem(STORAGE_KEY_ACTIVE_SESSION);
  if (currentActive === sessionId) {
    const nextActive = sessions[0].id;
    localStorage.setItem(STORAGE_KEY_ACTIVE_SESSION, nextActive);
    return nextActive;
  }
  return currentActive;
}

/**
 * 存储超出限额时清理老旧会话
 */
function pruneOldSessions() {
  const sessions = listSessions();
  if (sessions.length > 1) {
    const oldest = sessions.pop();
    if (oldest) {
      localStorage.removeItem(PREFIX_SESSION_MSGS + oldest.id);
      saveSessionsMeta(sessions);
    }
  }
}
