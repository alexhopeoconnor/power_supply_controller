import { useContext } from 'react';
import { Link } from "react-router-dom";
import './App.css';
import { CurrentStatusContext } from './contexts/current-status-context'

function App() {
  const currentStatus = useContext(CurrentStatusContext);
  return (
    <div className="App">
      
    </div>
  );
}

export default App;
