import { storageApi } from '../../api/storage.js';
import { showToast } from '../../utils/toast.js';
import { formatBytes } from '../../utils/helpers.js';

function escapeHtml(s) {
  if (!s) return '';
  return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

let curSdPath = '/';

export const storageView = {
  updateStorageInfo: (st) => {
    if (!st) return;
    const dataDir = document.getElementById('dataBaseDir');
    const tempDir = document.getElementById('tempBaseDir');
    if (st.data_dir && dataDir) dataDir.innerText = st.data_dir;
    if (st.temp_dir && tempDir) tempDir.innerText = st.temp_dir;

    const badge = document.getElementById('sdcardMountBadge');
    const pbar = document.getElementById('sdcardProgressBar');
    if (st.sdcard_mounted) {
      if (badge) {
        badge.innerText = '● 已挂载就绪';
        badge.style.color = 'var(--accent-sub)';
      }
      document.getElementById('sdcardMountPoint').innerText = st.sdcard_mount_point || '/mnt/sdcard';
      document.getElementById('sdcardUsedPct').innerText = (st.sdcard_used_pct || 0) + '%';
      if (pbar) pbar.style.width = Math.min(100, Math.max(0, st.sdcard_used_pct || 0)) + '%';
      const totalMb = st.sdcard_total_mb || 0;
      const freeMb = st.sdcard_free_mb || 0;
      const usedMb = totalMb >= freeMb ? (totalMb - freeMb) : 0;
      document.getElementById('sdcardTotalMb').innerText = totalMb;
      document.getElementById('sdcardUsedMb').innerText = usedMb;
    } else {
      if (badge) {
        badge.innerText = '○ 未检测到 TF 卡';
        badge.style.color = 'var(--muted)';
      }
      if (pbar) pbar.style.width = '0%';
    }
  },

  sdcardRefreshList: async () => {
    const curPathEl = document.getElementById('currentPathDisplay');
    const tbody = document.getElementById('sdcardFileList');
    if (curPathEl) curPathEl.innerText = curSdPath;

    try {
      const d = await storageApi.getList(curSdPath);
      if (!tbody) return;

      if (!d.success || !d.items) {
        tbody.innerHTML = '<tr><td colspan="3" style="padding:16px; text-align:center; color:#ff5555">读取失败: ' + (d.error || '未知错误') + '</td></tr>';
        return;
      }
      if (d.items.length === 0) {
        tbody.innerHTML = '<tr><td colspan="3" style="padding:16px; text-align:center; color:var(--muted)">当前目录为空</td></tr>';
        return;
      }

      d.items.sort((a, b) => (b.is_dir - a.is_dir) || a.name.localeCompare(b.name));

      let html = '';
      d.items.forEach(it => {
        const fullItemPath = (curSdPath === '/' ? '' : curSdPath) + '/' + it.name;
        const icon = it.is_dir ? '📁' : '📄';
        const sizeStr = it.is_dir ? '<span style="color:var(--muted)">[目录]</span>' : formatBytes(it.size);

        let actions = '';
        if (!it.is_dir) {
          actions += `<a href="/api/sdcard/download?path=${encodeURIComponent(fullItemPath)}" download="${it.name}" style="color:var(--accent); text-decoration:none; margin-right:8px;">⬇️下载</a>`;
        }
        actions += `<a href="javascript:void(0)" data-sd-delete="${escapeHtml(fullItemPath)}" data-sd-isdir="${it.is_dir ? '1' : '0'}" style="color:#ff6b6b; text-decoration:none;">🗑️删除</a>`;

        const nameCol = it.is_dir
          ? `<span data-sd-nav="${escapeHtml(fullItemPath)}" style="cursor:pointer; color:var(--accent); font-weight:600;">${escapeHtml(it.name)}</span>`
          : `<span style="color:#e6edf3;">${escapeHtml(it.name)}</span>`;

        html += `
          <tr style="border-bottom:1px solid rgba(255,255,255,0.04);">
            <td style="padding:8px 12px;"><span style="margin-right:6px;">${icon}</span>${nameCol}</td>
            <td style="padding:8px 12px; color:var(--muted);">${sizeStr}</td>
            <td style="padding:8px 12px; text-align:right;">${actions}</td>
          </tr>
        `;
      });
      tbody.innerHTML = html;

      tbody.querySelectorAll('[data-sd-nav]').forEach(el => {
        el.addEventListener('click', () => storageView.sdcardGoPath(el.dataset.sdNav));
      });
      tbody.querySelectorAll('[data-sd-delete]').forEach(el => {
        el.addEventListener('click', () => storageView.sdcardDelete(el.dataset.sdDelete, el.dataset.sdIsdir === '1'));
      });
    } catch (e) {
      if (tbody) tbody.innerHTML = '<tr><td colspan="3" style="padding:16px; text-align:center; color:#ff5555">无法连接至开发板端侧服务</td></tr>';
    }
  },

  sdcardGoPath: (p) => {
    curSdPath = p.startsWith('/') ? p : '/' + p;
    storageView.sdcardRefreshList();
  },

  sdcardGoUp: () => {
    if (curSdPath === '/' || !curSdPath) return;
    const parts = curSdPath.split('/').filter(Boolean);
    parts.pop();
    curSdPath = '/' + parts.join('/');
    storageView.sdcardRefreshList();
  },

  sdcardMkdir: async () => {
    const inp = document.getElementById('newDirName');
    const name = inp?.value.trim();
    if (!name) return;
    const target = (curSdPath === '/' ? '' : curSdPath) + '/' + name;
    try {
      const d = await storageApi.mkdir(target);
      if (d.success) {
        if (inp) inp.value = '';
        storageView.sdcardRefreshList();
      } else {
        alert('创建失败: ' + (d.error || '权限或路径错误'));
      }
    } catch (e) {
      alert('创建失败: ' + e.message);
    }
  },

  sdcardUploadText: async () => {
    const nameInp = document.getElementById('uploadFileName');
    const contentInp = document.getElementById('uploadFileContent');
    const fname = nameInp?.value.trim();
    const content = contentInp?.value;
    if (!fname) { alert('请输入目标文件名'); return; }
    const target = (curSdPath === '/' ? '' : curSdPath) + '/' + fname;

    try {
      const d = await storageApi.uploadText(target, content);
      if (d.success) {
        showToast('✅ 文件已写入 TF 卡');
        if (nameInp) nameInp.value = '';
        if (contentInp) contentInp.value = '';
        storageView.sdcardRefreshList();
      } else {
        alert('保存失败: ' + (d.error || '未知错误'));
      }
    } catch (e) {
      alert('保存失败: ' + e.message);
    }
  },

  handleLocalFileSelected: (input) => {
    if (input.files && input.files[0]) {
      const file = input.files[0];
      const nameInp = document.getElementById('uploadFileName');
      const contentInp = document.getElementById('uploadFileContent');
      if (nameInp) nameInp.value = file.name;
      const reader = new FileReader();
      reader.onload = function(e) {
        if (contentInp) contentInp.value = e.target.result;
      };
      reader.readAsText(file);
    }
  },

  sdcardDelete: async (p, isDir) => {
    if (!confirm(`确认永久删除该${isDir ? '目录' : '文件'}：\n${p} ？`)) return;
    try {
      const d = await storageApi.deleteItem(p);
      if (d.success) storageView.sdcardRefreshList();
      else alert('删除失败: ' + (d.error || '非空或权限不足'));
    } catch (e) {
      alert('删除失败: ' + e.message);
    }
  }
};
