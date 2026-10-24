# OStrands 0.8.0 Hybrid — 변경 요약

최종 설계와 빌드 절차는 [README-HYBRID-KO.md](README-HYBRID-KO.md)를 확인하세요.

요약:
- 공식 0.8.0 기반 + 크리처 지원 + 한국어 메뉴
- SexLab P+ 2.19.0에서 `sslActorAlias.GetOrgasmCount()` 폴백 사용
- 독립형 `SLSO.esp`는 필요하지 않음
- PEX / ESP / ESL 파일을 덮어쓰거나 추가하지 않음
- 10초 액터별 반복 제한 유지
- 카운터 증가분은 계산하되, 한 번의 관측에서 여러 증가가 나타나면 효과 요청을 하나로 병합
- Windows/MSVC 빌드 및 게임 내 검증은 아직 미완료
