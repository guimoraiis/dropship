# Network Monitor — OW2 Route Analysis

Companion script para o **dropship auto-block**. Monitora rotas de rede em tempo real durante partidas do Overwatch 2, registrando pathping, latência por hop e packet loss.

## 🎯 Objetivo

Capturar visibilidade completa da conexão durante gameplay:
- ✅ IP ativo conectado
- ✅ Rota completa (tracert/pathping)
- ✅ Latência + packet loss por hop
- ✅ Correlação com dropship auto-block events
- ✅ Dataset estruturado para análise / LLM training

## 📋 Requisitos

- **Windows 10+** com PowerShell 5.1+
- **Admin privileges** (para pathping funcionar)
- **Overwatch 2** instalado e runável
- **Dropship** rodando com auto-block ativado

## 🚀 Instalação & Uso

### 1. Abra PowerShell como Admin

```powershell
# Clone ou navegue até:
cd C:\Users\guife\Documents\DropShip2

# Execute o script:
.\network-monitor.ps1
```

### 2. Parâmetros (Opcionais)

```powershell
# Usar diretório de output customizado
.\network-monitor.ps1 -OutputPath "C:\Temp\ow2-logs"

# Reduzir interval de check (mais frequente = mais CPU)
.\network-monitor.ps1 -CheckInterval 3

# Aumentar max hops para pathping
.\network-monitor.ps1 -PathpingMaxHops 20
```

### 3. Durante o Jogo

- Script detecta automaticamente quando OW2 abre
- Começa a monitorar conexão ativa
- Executa pathping quando IP muda (detecta novo server)
- Registra em JSON estruturado
- Sai automaticamente quando OW2 fecha

## 📊 Output Files

```
%TEMP%\dropship\network-monitor\
├── network-monitor.log          # Log em tempo real
├── routes.json                   # Dados estruturados (pathping + latência)
└── correlation-report.txt        # Comparação com dropship auto-block log
```

### Exemplo de routes.json

```json
{
  "timestamp": "2026-06-30 14:25:30.123",
  "uptime_seconds": 45.5,
  "active_connection": {
    "remote_ip": "34.39.128.45",
    "remote_port": "7777",
    "server_region": "GBR1",
    "local_addr": "192.168.1.100:54321"
  },
  "pathping": {
    "target_ip": "34.39.128.45",
    "max_hops": 15,
    "hops": [
      {
        "hop_num": 1,
        "ip": "192.168.1.1",
        "min_ms": 1,
        "avg_ms": 2,
        "max_ms": 3,
        "loss_pct": 0
      },
      {
        "hop_num": 3,
        "ip": "peering.as64512.br",
        "min_ms": 40,
        "avg_ms": 45,
        "max_ms": 55,
        "loss_pct": 15
      },
      {
        "hop_num": 4,
        "ip": "34.39.128.45",
        "min_ms": 110,
        "avg_ms": 120,
        "max_ms": 140,
        "loss_pct": 2
      }
    ]
  },
  "degradation": {
    "detected": true,
    "degraded_hop": {
      "hop_num": 3,
      "ip": "peering.as64512.br",
      "loss_pct": 15,
      "avg_ms": 45
    },
    "recommendation": "Degradation at hop 3 (peering.as64512.br): loss=15%, latency=45ms"
  }
}
---
```

## 🔄 Correlação com Dropship

O script automaticamente compara com logs do dropship:

```
dropship auto-block log:
2026-06-30 01:37:30 | Brazil | 34.39.128.0 | packet loss 22%

network-monitor routes.json:
{...
  "degraded_hop": {
    "hop_num": 3,
    "ip": "peering.as64512.br",
    "loss_pct": 15
  }
}

correlation-report.txt:
✓ Correlação encontrada!
  Auto-block disparou quando hop 3 tinha 15% loss
  Confirma degradação de rota (não falha de servidor)
```

## 📈 Análise Posterior

Após rodar várias partidas, você terá:

1. **routes.json** — histórico completo de rotas + degradações
2. **correlation-report.txt** — eventos que causaram auto-block
3. **network-monitor.log** — detalhes de cada monitoramento

Análise:
- **Qual hop está degradado?** → Identifica ISP/peering problemático
- **Em que horários?** → Padrão de degradação
- **Antes/depois ExitLag** → Confirma rota alternativa funciona

## 🛠️ Troubleshooting

### "Access Denied" ao executar
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### Pathping não funciona
- Requer admin privileges
- Se não tiver, script continua mas com dados limitados
- Alguns ISPs bloqueiam ICMP — use `-Verbose` pra debug

### Não detecta OW2
```powershell
# Verifique nome do processo:
Get-Process | Where-Object Name -match "overwatch"

# Customize se necessário:
.\network-monitor.ps1 -TargetProcess "OverwatchLauncher"
```

## 🚀 Próximos Passos (v2)

- [ ] Integrar dados na UI do dropship (mostrar rota visualmente)
- [ ] Sugestões de IPs alternativos para contornar degradação
- [ ] Export para dataset (treinar LLM de "best path selection")
- [ ] Análise estatística (horários de pico, ISPs problemáticos)

## 📝 Exemplo de Uso Completo

```powershell
# Terminal 1: Abra Overwatch
Start-Process "C:\Program Files\Overwatch\Overwatch.exe"

# Terminal 2 (Admin): Inicie o monitor
cd C:\Users\guife\Documents\DropShip2
.\network-monitor.ps1

# Jogue umas 3-5 matches Brasil
# O monitor rodará em background

# Após fechar OW2:
# Verifique os logs em %TEMP%\dropship\network-monitor\
# Analise routes.json + correlation-report.txt
```

## 📊 Correlação com Auto-Block

```
Timeline da partida:

14:25:00 — Script detecta conexão em 34.39.128.45 (GBR1)
14:25:05 — Pathping: hop 3 = 15% loss, 45ms
14:25:10 — Dropship detecta 20% loss no Brasil
14:25:11 — Dropship auto-blocks GBR1 (você desconecta)
14:25:12 — Reconecta em novo server
14:25:30 — Nova rota: hop 3 = 2% loss, 20ms (sem degradação)

✓ Correlação valida: auto-block funcionou!
```

## 💡 Use Cases

1. **Diagnóstico**: Saber exatamente qual hop está ruins
2. **ISP Escalation**: "Seu peering em hop 3 tem 15% loss"
3. **VPN Testing**: Antes/depois ExitLag com dados concretos
4. **LLM Training**: Dataset de rotas + degradações reais
5. **Rota Alternativa**: Descobrir ISP que funciona melhor

---

**Autor**: Claude + Guimoraiis  
**Versão**: 1.0  
**Data**: 2026-06-30
