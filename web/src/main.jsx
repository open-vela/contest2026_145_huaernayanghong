import React from 'react';
import ReactDOM from 'react-dom/client';
import { App } from './App.jsx';

// 全局样式体系
import './styles/variables.css';
import './styles/base.css';
import './styles/components.css';
import './styles/views/agent.css';
import './styles/views/terminal.css';

const rootElement = document.getElementById('root');
if (rootElement) {
  ReactDOM.createRoot(rootElement).render(
    <React.StrictMode>
      <App />
    </React.StrictMode>
  );
}
