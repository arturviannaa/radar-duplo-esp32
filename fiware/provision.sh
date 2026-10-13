#!/usr/bin/env bash
# Provisiona o service group e o device "radar001" no IoT Agent JSON.
# Rode uma vez, depois de `docker compose up -d`, antes de ligar o ESP32.
set -euo pipefail

IOTA_HOST="${IOTA_HOST:-localhost}"
IOTA_PORT="${IOTA_PORT:-4041}"
ORION_HOST="${ORION_HOST:-localhost}"
ORION_PORT="${ORION_PORT:-1026}"

# Multi-tenant do FIWARE: tudo neste exemplo vive dentro do servico "radar".
SERVICE="radar"
SERVICE_PATH="/"
APIKEY="radar123"
DEVICE_ID="radar001"
ENTITY_TYPE="Radar"

echo "==> Cria service group (API key=${APIKEY})"
curl -sS -o /dev/null -w "  HTTP %{http_code}\n" -X POST \
  "http://${IOTA_HOST}:${IOTA_PORT}/iot/services" \
  -H "fiware-service: ${SERVICE}" \
  -H "fiware-servicepath: ${SERVICE_PATH}" \
  -H "Content-Type: application/json" \
  -d @- <<JSON
{
  "services": [{
    "apikey": "${APIKEY}",
    "cbroker": "http://orion:1026",
    "entity_type": "${ENTITY_TYPE}",
    "resource": ""
  }]
}
JSON

echo "==> Provisiona device ${DEVICE_ID}"
curl -sS -o /dev/null -w "  HTTP %{http_code}\n" -X POST \
  "http://${IOTA_HOST}:${IOTA_PORT}/iot/devices" \
  -H "fiware-service: ${SERVICE}" \
  -H "fiware-servicepath: ${SERVICE_PATH}" \
  -H "Content-Type: application/json" \
  -d @- <<JSON
{
  "devices": [{
    "device_id": "${DEVICE_ID}",
    "entity_name": "urn:ngsi-ld:Radar:${DEVICE_ID}",
    "entity_type": "${ENTITY_TYPE}",
    "transport": "MQTT",
    "protocol": "json",
    "attributes": [
      { "object_id": "d1", "name": "distanciaEsquerda", "type": "Number" },
      { "object_id": "d2", "name": "distanciaDireita",  "type": "Number" }
    ]
  }]
}
JSON

echo
echo "Pronto. Para checar no Orion depois que o ESP32 publicar:"
echo "  curl -s http://${ORION_HOST}:${ORION_PORT}/v2/entities/urn:ngsi-ld:Radar:${DEVICE_ID} \\"
echo "    -H 'fiware-service: ${SERVICE}' -H 'fiware-servicepath: ${SERVICE_PATH}' | jq"
