let backendHost;

const hostname = window && window.location && window.location.hostname;

if(hostname === 'localhost') {
  backendHost = 'http://192.168.15.64';
} else {
  backendHost = '';
}

export const API_ENDPOINT = backendHost;