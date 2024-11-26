# 1. Introdução:

Este relatório abordará os resultados 

# 2. Objetivo
Analisar diferentes técnicas de escalonamento de processos e seu impacto no desempenho do sistema operacional. Entender como diferentes configurações podem afetar a velocidade do sistema operacional.

# 3. Descrição do experimento
Foi desenvolvido um sistema operacional rudimentar para a máquina virtual fornecida pelo professor. Neste sistema operacional foram implementados todos os requisitos especificados no arquivo README.md.

## 3.1. Escalonadores
Foram implementados três tipos de escalonadores no sistema operacional.

### Escalonador Simples
Este escalonador escolhe o primeiro processo pronto da tabela de processos. Enquanto o processo corrente pode executar, ele vai continuar executando, não havendo nenhum tipo de preempção. Caso ele não possa mais executar, seja por um bloqueio ou pela morte do mesmo, o próximo processo pronto na tabela de processos será executado.

#### Resultados (intervalo = 50):
| N. Processos | T. Execução |  T. Ocioso | N. Preempções |
|-|-|-|-|
| 4 | 27885 | 4347 | 0 |

| N. IRQ Reset | N. IRQ Erro CPU | N. IRQ Chamada Sistema| N. IRQ Relógio | N. IRQ Teclado | N. IRQ Tela |
|-|-|-|-|-|-|
| 1 | 0 | 462 | 556 | 0 | 0 |

| ID | N. Preempções | T. Retorno | T. Resposta | V. Pronto | T. Pronto | V. Exec. | T. Exec. | V. Bloq. | T. Bloq. |
|-|-|-|-|-|-|-|-|-|-|
| 1 | 0 | 27869 | 7 | 4 | 29 | 4 | 760 | 3 | 27080 |
| 4 | 0 | 27162 | 97 | 131 | 12758 | 131 | 9576 | 130 | 4828 |
| 3 | 0 | 15770 | 507 | 21 | 10660 | 21 | 4023 | 20 | 1087 |
| 2 | 0 | 13412 | 560 | 7 | 3925 | 7 | 9179 | 6 | 308 |

### Escalonador Circular (Round-robin)
Este escalonador é um escalonador preemptivo que busca distribuir carga igualmente entre processos. Cada processo possui um intervalo de tempo, o _quantum_, que determina o tempo restante que o processo possui para executar. Em cada interrupção de relógio, o quantum do processo corrente diminui. Quando seu quantum chegar a 0, o processo volta ao final da fila de processos e seu _quantum_ é reiniciado.

#### Resultados (intervalo = 50, quantum = 5):
| N. Processos | T. Execução |  T. Ocioso | N. Preempções |
|-|-|-|-|
| 4 | 24049 | 2431 | 58 |

| N. IRQ Reset | N. IRQ Erro CPU | N. IRQ Chamada Sistema| N. IRQ Relógio | N. IRQ Teclado | N. IRQ Tela |
|-|-|-|-|-|-|
| 1 | 0 | 462 | 479 | 0 | 0 |

| ID | N. Preempções | T. Retorno | T. Resposta | V. Pronto | T. Pronto | V. Exec. | T. Exec. | V. Bloq. | T. Bloq. |
|-|-|-|-|-|-|-|-|-|-|
| 1 | 2 | 24033 | 0 | 5 | 0 | 5 | 740 | 2 | 23293 |
| 4 | 5 | 23226 | 99 | 110 | 10909 | 110 | 8537 | 104 | 3880 |
| 2 | 39 | 17095 | 178 | 48 | 8576 | 48 | 8058 | 8 | 461 |
| 3 | 12 | 11984 | 268 | 26 | 6976 | 26 | 4284 | 13 | 725 |

### Escalonador prioritário
Este escalonador é um escalonador preemptivo semelhante ao escalonador circular. Nele, cada processo possui, além do _quantum_, uma prioridade, que é calculada dinamicamente. Todo processo é criado com prioridade = 0.5 e, quando perde o processador, seja por bloqueio ou por preempção, a sua prioridade é recalculada. O cálculo da prioridade segue a fórmula:
`prio = (prio + t_exec/t_quantum) / 2`
onde `t_exec` é o tempo desde o início da execução do processo.

#### Resultados (intervalo = 50, quantum = 5):
| N. Processos | T. Execução |  T. Ocioso | N. Preempções |
|-|-|-|-|
| 4 | 22902 | 1828 | 59 |

| N. IRQ Reset | N. IRQ Erro CPU | N. IRQ Chamada Sistema| N. IRQ Relógio | N. IRQ Teclado | N. IRQ Tela |
|-|-|-|-|-|-|
| 1 | 0 | 462 | 456 | 0 | 0 |

| ID | N. Preempções | T. Retorno | T. Resposta | V. Pronto | T. Pronto | V. Exec. | T. Exec. | V. Bloq. | T. Bloq. |
|-|-|-|-|-|-|-|-|-|-|
| 1 | 2 | 22886 | 0 | 5 | 0 | 5 | 740 | 2 | 22146 |
| 4 | 5 | 22179 | 97 | 101 | 9810 | 101 | 8578 | 95 | 3791 |
| 2 | 39 | 17748 | 190 | 48 | 9124 | 48 | 8163 | 8 | 461 |
| 3 | 12 | 11487 | 269 | 27 | 7284 | 27 | 3593 | 13 | 610 |

