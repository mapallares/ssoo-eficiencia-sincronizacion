#!/bin/bash

declare -A PROGRAMAS=(
  [1]="barreras"
  [2]="condVar"
)

# Parámetros a probar
NTHREADS=(2 5 10)
NGENERACIONES=(10 50 100)
NPOBLACION=(10 100 1000)

RESULTADOS_DIR="resultados"
mkdir -p "$RESULTADOS_DIR"

# Crear archivos CSV por versión
ENCABEZADO="Version,NTHREADS,NGENERACIONES,NPOBLACION,TiempoReal,TiempoUser,TiempoSys,UsageCPU,Throughput,Latencia"

declare -A CSV
for ver in "${!PROGRAMAS[@]}"; do
  CSV[$ver]="$RESULTADOS_DIR/metrics_v${ver}.csv"
  echo "$ENCABEZADO" > "${CSV[$ver]}"
done

# Compilar versiones del programa
echo "Compilando versiones..."
for ver in "${!PROGRAMAS[@]}"; do
  gcc "${PROGRAMAS[$ver]}.c" -o "${PROGRAMAS[$ver]}"
done

# Función para ejecutar y medir los programas
function calcularMetricas {
  local ver=$1
  local threads=$2
  local gens=$3
  local poblacion=$4
  local bin="./${PROGRAMAS[$ver]}"

  # Ejecutar y capturar las métricas con GNU time
  METRICAS=$( /usr/bin/time -f "%e;%U;%S;%P" "$bin" "$threads" "$gens" "$poblacion" 2>&1 1>/dev/null )
  IFS=';' read -r tiempo_real tiempo_user tiempo_sys uso_cpu <<< "$METRICAS"

  # Calcular el throughput y la latencia
  total_individuos=$((gens * poblacion))
  export LC_NUMERIC=C

  throughput=$(LC_NUMERIC=C echo "scale=8; $total_individuos / $tiempo_real" | bc -l)
  latencia=$(LC_NUMERIC=C echo "scale=12; $tiempo_real / $total_individuos" | bc -l)

  if (( $(echo "$tiempo_real == 0" | bc -l) )); then
      throughput="0.0000"
      latencia="0.00000000"
  else
      throughput=$(awk "BEGIN { printf \"%.4f\", $total_individuos / $tiempo_real }")
      latencia=$(awk "BEGIN { printf \"%.8f\", $tiempo_real / $total_individuos }")
  fi

  # Registrar los resultados
  echo "$ver,$threads,$gens,$poblacion,$tiempo_real,$tiempo_user,$tiempo_sys,$uso_cpu,$throughput,$latencia" >> "${CSV[$ver]}"
}

# Cálculo de las métricas para cada configuración
echo "Iniciando pruebas..."
for ver in "${!PROGRAMAS[@]}"; do
  for threads in "${NTHREADS[@]}"; do
    for gens in "${NGENERACIONES[@]}"; do
      for poblacion in "${NPOBLACION[@]}"; do
        echo "Versión $ver (${PROGRAMAS[$ver]}): Hilos=$threads, Gen=$gens, Pob=$poblacion"
        calcularMetricas "$ver" "$threads" "$gens" "$poblacion"
      done
    done
  done
done

echo "Pruebas completadas."
echo "Resultados guardados en:"
for ver in "${!PROGRAMAS[@]}"; do
  echo "   - ${CSV[$ver]}"
done