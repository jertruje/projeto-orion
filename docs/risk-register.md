# Registro inicial de riscos

Escala qualitativa não é probabilidade estatística: Alta/Média/Baixa expressa
prioridade para mitigação neste estágio.

| ID | Risco | Impacto | Prioridade | Mitigação/contingência | Evidência de fechamento |
|---|---|---|---|---|---|
| R-01 | Parser ELF manual aceitar segmentação inválida/overflow. | Corrupção de memória ou transferência indevida. | Alta | Validar todos bounds/overflow/alinhamentos e tipos; fuzz/negative tests; limitar formato. | Testes de parser + review; rejeição determinística de entradas malformadas. |
| R-02 | Contrato de boot incompleto ou lifetime de mapa incorreto. | Uso de memória/referências inválidas após EBS. | Alta | Documentar ABI/lifetime/regions; validar descriptor size/count e reservas antes do allocator. | Testes de contrato e auditoria das regiões carregadas. |
| R-03 | Falta de IDT/handlers próprios. | Exceção gera triple fault/reboot ou estado sem diagnóstico. | Alta | Fase 7 antes de habilitar interrupções/timer. | Exceções controladas observadas no QEMU. |
| R-04 | Ausência de alocador/page tables. | Sem base segura para processos e drivers. | Alta | Implementar e testar Fase 8 com reservas e invariantes. | Testes de allocate/free/map/unmap e fault handling. |
| R-05 | Complexidade de capacidade/microkernel subestimada. | Atraso, falhas no modelo de autoridade/IPC. | Alta | Completar pesquisa e protótipos; manter ADR provisória. | ADR fase 3 e resultados reproduzíveis de IPC/isolamento. |
| R-06 | Alegações de segurança/desempenho baseadas só em QEMU. | Conclusão enganosa ou risco aos usuários. | Alta | Separar QEMU de hardware; aplicar protocolo Linux e reportar limitações. | Relatórios com método, dados brutos e revisão independente. |
| R-07 | Toolchain externa ao repositório e dependente do perfil Windows. | Build não reproduzível noutro host. | Média | Fixar versões, documentar bootstrap, CI host e hashes de artefato. | Build limpo num ambiente independente. |
| R-08 | Mudança/instabilidade de especificação e firmware UEFI. | Boot incompatível. | Média | Versões de spec, teste em mais de uma implementação; fallback claro. | Matriz firmware testada. |
| R-09 | Atualização/armazenamento sem transação. | Corrupção ou dispositivo não inicia. | Alta para release | Não oferecer atualização no protótipo; planejar A/B/journal e power-cut tests. | Campanha de interrupção e validação pós-boot. |
| R-10 | Erro humano de integração/licença/origem de código. | Risco legal e violação do requisito de kernel próprio. | Média | Registrar referências, autoria e origem; não incorporar código de kernel. | Revisão de origem/licenças por release. |
