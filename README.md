# OStrands 0.8.0 Hybrid — Creature + SexLab P+ 2.19.0

**실험용 DLL 소스 프로젝트**입니다. 컴파일된 DLL은 포함하지 않습니다.

- 공식 OStrands 0.8.0 기반
- 크리처 지원과 한국어 메뉴 유지
- SexLab P+ 2.19.0의 `sslActorAlias.GetOrgasmCount()` 폴링 대체 경로
- 독립형 `SLSO.esp` 감지에 의존하지 않음
- PEX/ESP/ESL을 수정하거나 추가하지 않음
- SKSE 내부 `ModEvent.Send` 함수 직접 후킹을 사용하지 않음
- 액터별 기존 10초 재요청 제한 유지

한국어 적용·검증 안내: [README-HYBRID-KO.md](README-HYBRID-KO.md)

현재 환경에서 Windows/MSVC 컴파일 및 게임 내 검증은 수행하지 않았습니다. GitHub Actions 빌드가 성공한 뒤 MO2의 별도 테스트 모드에서 검증하세요.
